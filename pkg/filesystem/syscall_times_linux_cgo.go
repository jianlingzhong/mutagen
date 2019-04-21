// +build linux,cgo

package filesystem

/*
#include <sys/stat.h>
*/
import "C"

// extractCModificationTime is a convenience function for extracting the
// modification time specification (decomposed into second and nanosecond
// components) from a C.struct_stat structure. It's necessary since not all
// POSIX platforms use the same struct field name for this value.
func extractCModificationTime(metadata *C.struct_stat) (int64, int64) {
	return int64(metadata.st_mtim.tv_sec), int64(metadata.st_mtim.tv_nsec)
}
