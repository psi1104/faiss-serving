#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs {

class Mapper {
 public:
  Mapper() : fileDescriptor(-1), mmapPointer(static_cast<char*>(MAP_FAILED)) {}
  ~Mapper() {
    if (mmapPointer != MAP_FAILED)
      ::munmap(mmapPointer, fileStat.st_size);

    if (fileDescriptor != -1)
      ::close(fileDescriptor);
  }

  void open(const std::string filename) {
    fileDescriptor = ::open(filename.c_str(), O_RDONLY);
    if (fileDescriptor == -1)
      throw std::runtime_error("Cannot read file " + filename);

    if (::fstat(fileDescriptor, &fileStat) == -1) {
      // close file
      ::close(fileDescriptor);
      throw std::runtime_error("fstat error");
    }

    mmapPointer = reinterpret_cast<char*>(::mmap(0, fileStat.st_size, PROT_READ, MAP_SHARED, fileDescriptor, 0));

    if (mmapPointer == MAP_FAILED) {
      ::close(fileDescriptor);
      throw std::runtime_error("Error mmapping the file");
    }

    size_t startIndex = 0;
    for (size_t i = 0; i < fileStat.st_size; i++) {
      if (mmapPointer[i] == '\n') {
        seekPoints.push_back(std::make_pair(startIndex, i - startIndex));
        startIndex = i + 1;  // skip new line
      }
    }

    numRows = seekPoints.size();
  }

  bool isOpened() const { return mmapPointer != MAP_FAILED && fileDescriptor != -1; }
  size_t getNumRows() const { return numRows; }
  std::string getItem(size_t index) {
    if (index >= numRows)
      throw std::invalid_argument("index >= numRows");

    auto offset = seekPoints.at(index);
    return std::string(mmapPointer + offset.first, offset.second);
  }

 private:
  // offset, size
  std::vector<std::pair<size_t, size_t>> seekPoints;
  size_t numRows;
  int fileDescriptor;
  struct stat fileStat;
  char* mmapPointer;
};

}  // namespace fs
