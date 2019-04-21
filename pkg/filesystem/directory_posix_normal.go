// +build darwin,!cgo linux,!cgo !darwin,!linux

package filesystem

// haveFastDirectoryContents encodes whether or not the platform supports
// C-based directory content listing.
const haveFastDirectoryContents = false

// readDirectoryContentNamesFast is unavailable on this platform.
func readDirectoryContentNamesFast(directory int) ([]string, error) {
	panic("unsupported function")
}

// readDirectoryContentsFast is unavailable on this platform.
func readDirectoryContentsFast(directory int) ([]*Metadata, error) {
	panic("unsupported function")
}
