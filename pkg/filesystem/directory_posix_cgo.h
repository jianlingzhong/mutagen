#ifndef DIRECTORY_POSIX_CGO_H
#define DIRECTORY_POSIX_CGO_H

// Standard includes
#include <time.h>

// POSIX includes
#include <sys/stat.h>

// read_content_names is the C equivalent of Directory.ReadContentNames.
int read_content_names(int directory, const char ***names, int *count);

// free_content_names releases the name array created by read_content_names.
void free_content_names(const char **names, int count);

// read_contents is the C equivalent of Directory.ReadContents.
int read_contents(int directory,
                  const char ***names,
                  struct stat **metadata,
                  int *count);

// free_contents releases the arrays created by read_contents.
void free_contents(const char **names, struct stat *metadata, int count);

#endif // DIRECTORY_POSIX_CGO_H
