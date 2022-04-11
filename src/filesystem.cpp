#include "filesystem.hpp"

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/strings/str_split.h>
#include <absl/types/span.h>
#include <dirent.h>
#include <errno.h>
#include <glibmm/stringutils.h>
#include <glibmm/ustring.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

std::vector<File> GetFilesFromMockDirectory(
    absl::Span<const MockFile *const> mock_directory) {
  std::vector<File> file_names;
  std::transform(mock_directory.begin(), mock_directory.end(),
                 std::back_inserter(file_names), [](const MockFile *file) {
                   absl::StatusOr<File> new_file = File::Create(*file);
                   // Mock file creation cannot fail.
                   new_file.IgnoreError();
                   return new_file.value();
                 });
  return file_names;
}

absl::StatusOr<std::vector<File>> GetFileNamesFromLastMatchingDirectory(
    absl::Span<const MockFile *const> nested_directory_files,
    std::vector<std::string> &nested_directory_names) {
  // On no more nested directories, we should be on the last file.
  if (nested_directory_names.size() == 0)
    return GetFilesFromMockDirectory(nested_directory_files);

  std::string next_directory_name = *nested_directory_names.begin();
  nested_directory_names.erase(nested_directory_names.begin());

  // On empty last file, we are on the requested directory.
  if (next_directory_name.size() == 0)
    return GetFilesFromMockDirectory(nested_directory_files);

  const auto *next_directory =
      std::find_if(nested_directory_files.begin(), nested_directory_files.end(),
                   [&next_directory_name](const auto *file) {
                     return file->GetName() == next_directory_name;
                   });
  // Nullptr check needed since clang-tidy believes next_directory can be
  // nullptr, even though std::find_if() should return
  // absl::Span<const MockFile *const>::end() if not found.
  if (next_directory == nullptr ||
      next_directory == nested_directory_files.end() ||
      *next_directory == nullptr)
    return absl::NotFoundError("File or directory did not exist!");

  // The path entered may contain a file instead of a directory.
  const auto *next_directory_files =
      dynamic_cast<const MockDirectory *>(*next_directory);
  if (next_directory_files == nullptr) {
    absl::StatusOr<File> file = File::Create(**next_directory);
    // No errors possible when creating mock files.
    file.IgnoreError();
    return std::vector<File>({file.value()});
  }

  return GetFileNamesFromLastMatchingDirectory(next_directory_files->GetFiles(),
                                               nested_directory_names);
}

}  // namespace

File::File(std::string name, bool is_dir) : file_name_(name), is_dir_(is_dir) {}

absl::StatusOr<File> File::Create(dirent *file) {
  if (!file) return absl::InvalidArgumentError("file argument was nullptr");

  return File(file->d_name, file->d_type == DT_DIR);
}

absl::StatusOr<File> File::Create(const MockFile &file) {
  return File(file.GetName(),
              /*is_dir=*/dynamic_cast<const MockDirectory *>(&file) != nullptr);
}

bool File::operator==(const char *file_name) const {
  return GetName() == file_name;
}

std::string File::GetName() const { return file_name_; }
bool File::IsDirectory() const { return is_dir_; }

FileSystem::~FileSystem() {}

MockFileSystem::MockFileSystem(std::initializer_list<MockFile *> files)
    : root_("/", files) {}

MockFile::MockFile(const Glib::ustring &name) : name_(name) {}
MockFile::~MockFile() {}

std::string MockFile::GetName() const { return name_; }

MockDirectory::MockDirectory(const Glib::ustring &name,
                             std::initializer_list<MockFile *> files)
    : MockFile(name), files_(files) {}
MockDirectory::~MockDirectory() {}

absl::Span<const MockFile *const> MockDirectory::GetFiles() const {
  return files_;
}

absl::StatusOr<std::vector<File>> MockFileSystem::GetDirectoryFiles(
    const Glib::ustring &directory) const {
  std::vector<std::string> file_names;
  if (directory == "/") return GetFilesFromMockDirectory(root_.GetFiles());
  if (directory.empty())
    return absl::InvalidArgumentError("Directory cannot be empty!");

  // Since first character should be a /, the first string should be empty
  // after splitting it. Remove it from the list, since it is not needed.
  std::vector<std::string> nested_directory_names =
      absl::StrSplit(directory.c_str(), "/");
  std::string first_directory_name = *nested_directory_names.begin();
  nested_directory_names.erase(nested_directory_names.begin());
  if (first_directory_name.size() != 0)
    return absl::InvalidArgumentError(
        "Must be a full path starting with \"/\"!");

  return GetFileNamesFromLastMatchingDirectory(root_.GetFiles(),
                                               nested_directory_names);
}

// To test methods in POSIXFileSystem, make a test double that mocks POSIX APIs
// such as this:
// class MockPOSIXAPI : public POSIXAPIInterface {
//     DIR *opendir(const char *str) {}
//     dirrent *readdir(DIR *dir) { return dir->next_dir; }
// };
// Then pass that to POSIXFileSystem as a dependency for POSIXAPIInterface and
// use MockPOSIXAPI to ensure POSIXFileSystem::GetDirectoryFiles() returns the
// correct files.
absl::StatusOr<std::vector<File>> POSIXFileSystem::GetDirectoryFiles(
    const Glib::ustring &directory) const {
  DIR *dir = opendir(directory.c_str());
  if (dir == nullptr)
    return absl::NotFoundError(
        absl::StrCat("Can't open directory: ", strerror(errno)));

  std::vector<File> file_names;
  for (dirent *file = readdir(dir); file != nullptr; file = readdir(dir)) {
    std::string_view file_name = file->d_name;
    if (!(file->d_type & (DT_REG | DT_DIR)) || file_name == "." ||
        file_name == "..")
      continue;

    absl::StatusOr<File> new_file = File::Create(file);
    if (!new_file.ok()) continue;

    file_names.push_back(new_file.value());
  }

  return file_names;
}
