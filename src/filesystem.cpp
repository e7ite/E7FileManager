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

std::vector<std::string> GetFileNamesFromMockDirectory(
    absl::Span<const MockFile *const> mock_directory) {
  std::vector<std::string> file_names;
  std::transform(mock_directory.begin(), mock_directory.end(),
                 std::back_inserter(file_names),
                 [](const auto &file) { return file->GetName(); });
  return file_names;
}

absl::StatusOr<std::vector<std::string>> GetFileNamesFromLastMatchingDirectory(
    absl::Span<const MockFile *const> nested_directory_files,
    std::vector<std::string> &nested_directory_names) {
  // On no more nested directories, we should be on the last file.
  if (nested_directory_names.size() == 0)
    return GetFileNamesFromMockDirectory(nested_directory_files);

  std::string next_directory_name = *nested_directory_names.begin();
  nested_directory_names.erase(nested_directory_names.begin());

  // On empty last file, we are on the requested directory.
  if (next_directory_name.size() == 0)
    return GetFileNamesFromMockDirectory(nested_directory_files);

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
  if (next_directory_files == nullptr)
    return std::vector<std::string>({(*next_directory)->GetName()});

  return GetFileNamesFromLastMatchingDirectory(next_directory_files->GetFiles(),
                                               nested_directory_names);
}

}  // namespace

FileSystem::~FileSystem() {}

MockFileSystem::MockFileSystem(std::initializer_list<MockFile *> files)
    : root_("/", files) {}

MockFile::MockFile(Glib::UStringView name) : name_(name.c_str()) {}
MockFile::~MockFile() {}

std::string MockFile::GetName() const { return name_; }

MockDirectory::MockDirectory(Glib::UStringView name,
                             std::initializer_list<MockFile *> files)
    : MockFile(name), files_(files) {}
MockDirectory::~MockDirectory() {}

absl::Span<const MockFile *const> MockDirectory::GetFiles() const {
  return files_;
}

absl::StatusOr<std::vector<std::string>> MockFileSystem::GetDirectoryFiles(
    Glib::UStringView directory) const {
  std::vector<std::string> file_names;
  if (Glib::ustring("/") == directory.c_str())
    return GetFileNamesFromMockDirectory(root_.GetFiles());
  if (Glib::ustring("") == directory.c_str())
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
absl::StatusOr<std::vector<std::string>> POSIXFileSystem::GetDirectoryFiles(
    Glib::UStringView directory) const {
  DIR *dir = opendir(directory.c_str());
  if (dir == nullptr) return absl::NotFoundError("Error opening file");

  std::vector<std::string> file_names;
  for (dirent *file = readdir(dir); file != nullptr; file = readdir(dir))
    if (file->d_type == DT_REG) file_names.push_back(file->d_name);

  return file_names;
}
