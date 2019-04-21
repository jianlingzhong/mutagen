// +build !windows,cgo

package filesystem

import (
	"time"
	"unsafe"
)

/*
#include "directory_posix_cgo.h"
*/
import "C"

// haveFastDirectoryContents encodes whether or not the platform supports
// C-based directory content listing.
const haveFastDirectoryContents = true

// readDirectoryContentNamesFast is a fast C-based version of directory content
// name reading.
func readDirectoryContentNamesFast(directory int) ([]string, error) {
	// Call the C implementation.
	var names **C.char
	var count C.int
	if result, errno := C.read_content_names(C.int(directory), &names, &count); result < 0 {
		return nil, errno
	}

	// Convert values.
	namePointerSize := unsafe.Sizeof((*C.char)(unsafe.Pointer(uintptr(0))))
	results := make([]string, int(count))
	for i := 0; i < int(count); i++ {
		results[i] = C.GoString(*((**C.char)(unsafe.Pointer(uintptr(unsafe.Pointer(names)) + namePointerSize*uintptr(i)))))
	}

	// Release C memory.
	C.free_content_names(names, count)

	// Success.
	return results, nil
}

// readDirectoryContentsFast is a fast C-based version of directory content
// reading.
func readDirectoryContentsFast(directory int) ([]*Metadata, error) {
	// Call the C implementation.
	var names **C.char
	var metadata *C.struct_stat
	var count C.int
	if result, errno := C.read_contents(C.int(directory), &names, &metadata, &count); result < 0 {
		return nil, errno
	}

	// Convert values.
	namePointerSize := unsafe.Sizeof((*C.char)(unsafe.Pointer(uintptr(0))))
	results := make([]*Metadata, int(count))
	for i := 0; i < int(count); i++ {
		name := C.GoString(*((**C.char)(unsafe.Pointer(uintptr(unsafe.Pointer(names)) + namePointerSize*uintptr(i)))))
		metadatum := (*C.struct_stat)(unsafe.Pointer(uintptr(unsafe.Pointer(metadata)) + C.sizeof_struct_stat*uintptr(i)))
		results[i] = &Metadata{
			Name:             name,
			Mode:             Mode(metadatum.st_mode),
			Size:             uint64(metadatum.st_size),
			ModificationTime: time.Unix(extractCModificationTime(metadatum)),
			DeviceID:         uint64(metadatum.st_dev),
			FileID:           uint64(metadatum.st_ino),
		}
	}

	// Release C memory.
	C.free_contents(names, metadata, count)

	// Success.
	return results, nil
}
