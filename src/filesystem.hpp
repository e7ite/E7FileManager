#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <glibmm/ustring.h>

#include <string>
#include <vector>

class MockFile;

// Abstracted file object for all different supported file systems.
class File {
 public:
  static absl::StatusOr<File> Create(dirent *file);
  static absl::StatusOr<File> Create(const MockFile &file);

  std::string GetName() const;
  bool IsDirectory() const;

  bool operator==(const char *file_name) const;

 private:
  File(std::string name, bool is_dir);

  std::string file_name_;
  bool is_dir_;
};

// Abstraction layer that to interact with a file system. Provides method to
// list all files on a given directory and open, read, and write individual
// files on the file system.
class FileSystem {
 public:
  virtual ~FileSystem();

  // Obtains all the files existing in the directory specified by directory.
  // Must be a full path, no relative links will succeed. Returns an
  // absl::NotFoundError if the directory does not exist. Else will contain a
  // an array of file names that existed in the specified directory.
  virtual absl::StatusOr<std::vector<File>> GetDirectoryFiles(
      const Glib::ustring &directory) const = 0;
};

class MockFile {
 public:
  MockFile(const Glib::ustring &name);
  virtual ~MockFile();

  std::string GetName() const;

 private:
  Glib::ustring name_;
};

class MockDirectory : public MockFile {
 public:
  MockDirectory(const Glib::ustring &name,
                std::initializer_list<MockFile *> files);
  virtual ~MockDirectory();

  absl::Span<const MockFile *const> GetFiles() const;

 private:
  std::vector<MockFile *> files_;
};

// Can be used to test interactions with file systems. Supports making a file
// system intiialized with the MockFile and MockDirectory classes. Dynamic cast
// can be used to check if the stored object is a directory or a file.
//
// Directories can be accessed with Unix-like strings starting from /. Some
// examples include "/dog/gone/cat.txt". The files in these directories can be
// retrieved using GetDirectoryFiles(). Only full paths are supported.
//
// This does not handle crazy edge cases with path parsing. Some examples
// include full paths such as "//". Can also directly access the file system
// using GetRoot(), GetFiles(), MockDirectory::operator[], and so on.
//
// This also does not mock symbolic links, network connections, and so on. Only
// rudimentary files and directories.
//
// Can be initialized using brace initialization to form a tree-like directory
// structure similar to Unix..
class MockFileSystem : public FileSystem {
 public:
  MockFileSystem(std::initializer_list<MockFile *> files);

  virtual ~MockFileSystem() = default;

  absl::StatusOr<std::vector<File>> GetDirectoryFiles(
      const Glib::ustring &directory) const override;

  const MockDirectory &GetRoot() const { return root_; }

 private:
  MockDirectory root_;
};

// Interface for extracting files using POSIX APIs.
class POSIXFileSystem : public FileSystem {
 public:
  virtual ~POSIXFileSystem() = default;

  absl::StatusOr<std::vector<File>> GetDirectoryFiles(
      const Glib::ustring &directory) const override;
};

#endif
