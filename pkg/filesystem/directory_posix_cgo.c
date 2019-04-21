// +build darwin,cgo linux,cgo

#include "directory_posix_cgo.h"

// Standard includes
#include <stdlib.h>
#include <errno.h>
#include <string.h>

// POSIX includes
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

// _INITIAL_NAME_CAPACITY is the initial capacity to use for the name array. It
// should be set to a reasonable estimated average for directory content
// multiplicity in order to avoid reallocation.
#define _INITIAL_NAME_CAPACITY 15

// _NAME_CAPACITY_GROWTH_FACTOR is the rate at which name storage will grow.
#define _NAME_CAPACITY_GROWTH_FACTOR 2

int read_content_names(int directory, const char ***names, int *count) {
    // Allocate initial name storage. Because this array will be passed back to
    // Go, we need to make sure that any non-valid pointer slots are nulled out,
    // otherwise Go's garbage collector can get confused.
    const char **results = calloc(_INITIAL_NAME_CAPACITY, sizeof(const char *));
    if (results == NULL) {
        return -1;
    }
    int result_count = 0, result_capacity = _INITIAL_NAME_CAPACITY;

    // Duplicate the file descriptor since the combination of fdopendir and
    // closedir will result in its closure.
    if ((directory = fcntl(directory, F_DUPFD_CLOEXEC, 0)) < 0) {
        free_content_names(results, result_count);
        return -1;
    }

    // Open the directory for reading.
    DIR *iterator = fdopendir(directory);
    if (iterator == NULL) {
        close(directory);
        free_content_names(results, result_count);
    }

    // Zero-out errno so that we can track readdir errors.
    errno = 0;

    // Iterate over directory contents. We use readdir instead of readdir_r
    // because readdir is thread-safe (on different directory streams) in most
    // modern implementations and readdir_r has a number of design flaws (which
    // are detailed in the Linux readdir_r(3) documentation, but at the very
    // least include the potential for buffer overflows) and is deprecated on
    // Linux. We're better off using the safer function and simply ensuring that
    // we're on a platform where it's thread-safe.
    struct dirent *entry;
    while ((entry = readdir(iterator)) != NULL) {
        // Exclude names that reference the directory or its parent.
        if (strcmp(entry->d_name, ".") == 0) {
            continue;
        } else if (strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Resize storage if necessary.
        if (result_count == result_capacity) {
            // Compute the new capacity.
            result_capacity *= _NAME_CAPACITY_GROWTH_FACTOR;

            // Resize the allocation.
            const char **resized_results =
                realloc(results, result_capacity * sizeof(const char *));
            if (resized_results == NULL) {
                closedir(iterator);
                free_content_names(results, result_count);
                return -1;
            }
            results = resized_results;

            // Zero out any new pointer slots (see reason for this above).
            for (int i = result_count; i < result_capacity; i++) {
                results[i] = NULL;
            }
        }

        // Copy and store the name.
        char *name = malloc((strlen(entry->d_name) + 1) * sizeof(char));
        if (name == NULL) {
            closedir(iterator);
            free_content_names(results, result_count);
            return -1;
        }
        strcpy(name, entry->d_name);
        results[result_count] = name;

        // Increment the result count.
        result_count++;
    }

    // Check if the loop terminated due to an error.
    if (errno != 0) {
        int real_errno = errno;
        closedir(iterator);
        errno = real_errno;
        free_content_names(results, result_count);
        return -1;
    }

    // Rewind the directory handle so that any content cache is cleared (and the
    // underlying file descriptor seeks back to the beginning).
    rewinddir(iterator);

    // Close the directory handle.
    if (closedir(iterator) < 0) {
        free_content_names(results, result_count);
        return -1;
    }

    // For the sake of cleanliness, handle the case of no names by returning a
    // NULL pointer. This will still work fine with our free_content_names
    // function since free(NULL) is a no-op.
    if (result_count == 0) {
        free_content_names(results, result_count);
        results = NULL;
    }

    // Store results.
    *names = results;
    *count = result_count;

    // Success.
    return 0;
}

void free_content_names(const char **names, int count) {
    // Free individual names.
    for (int i = 0; i < count; i++) {
        free((void *)names[i]);
    }

    // Free the array itself.
    free(names);
}

int read_contents(int directory,
                  const char ***names,
                  struct stat **metadata,
                  int *count) {
    // Read names.
    const char **results_names;
    int result_count;
    if (read_content_names(directory, &results_names, &result_count) < 0) {
        return -1;
    }

    // Handle the case of no names, both for cleanliness and so we don't have to
    // tackle the ill-definedness of a length-0 allocation. This will work fine
    // with free_contents (and the underlying free_content_names) since
    // free(NULL) is a no-op. Our call to free_content_names isn't really
    // necessary here since results_names will be NULL, but it's better to be
    // consistent.
    if (result_count == 0) {
        free_content_names(results_names, result_count);
        *names = NULL;
        *metadata = NULL;
        *count = 0;
        return 0;
    }

    // Allocate metadata storage.
    struct stat *results_metadata = calloc(result_count, sizeof(struct stat));
    if (results_metadata == NULL) {
        free_content_names(results_names, result_count);
        return -1;
    }

    // Loop over names and grab metadata.
    for (int i = 0; i < result_count; i++) {
        if (fstatat(directory,
                    results_names[i],
                    &results_metadata[i],
                    AT_SYMLINK_NOFOLLOW) < 0) {
            // If the file has disappeared between listing and metadata querying
            // time, then just pretend that it never existed, because from an
            // observability standpoint, it may as well not have.
            if (errno == ENOENT) {
                free((void *)results_names[i]);
                for (int j = i; j < (result_count - 1); j++) {
                    results_names[j] = results_names[j+1];
                }
                results_names[result_count-1] = NULL;
                result_count--;
                i--;
                errno = 0;
                continue;
            }

            // Otherwise there's a more serious and unrecoverable failure.
            free_contents(results_names, results_metadata, result_count);
            return -1;
        }
    }

    // Store results.
    *names = results_names;
    *metadata = results_metadata;
    *count = result_count;

    // Success.
    return 0;
}

void free_contents(const char **names, struct stat *metadata, int count) {
    // Free the name array.
    free_content_names(names, count);

    // Free the metadata array.
    free(metadata);
}
