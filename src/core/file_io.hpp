#pragma once

#include <cstddef>
#include <fstream>
#include <memory>
#include <mutex>
#include <span>
#include <string>

namespace curse {

// expected to throw on fail
class FileReader {
public:
    virtual void ReadBytes(size_t beg, std::span<char> buf) = 0;

    virtual size_t GetSize() = 0;

    virtual ~FileReader() {}
};

// expected to throw on fail
class FileWriter {
public:
    virtual void Seek(size_t pos) = 0;

    virtual void Write(std::span<const char> bytes) = 0;

    // virtual void WriteAt(size_t beg, std::span<const char> bytes) = 0;

    virtual ~FileWriter() {}
};

class IfstreamReader : public FileReader {
private:
    std::ifstream m_file;

    void Error();

public:
    IfstreamReader(const std::string& file);

    void ReadBytes(size_t beg, std::span<char> buf) override;

    size_t GetSize() override;
};

class OfstreamWriter : public FileWriter {
private:
    std::ofstream m_file;

    void Error();

public:
    OfstreamWriter(const std::string& file);

    void Seek(size_t pos) override;

    void Write(std::span<const char> bytes) override;

    // void WriteAt(size_t pos, std::span<const char> bytes) override;
};

class ThreadSafeFileReader : public FileReader {
private:
    std::mutex m_mx;
    std::unique_ptr<FileReader> m_reader;

public:
    ThreadSafeFileReader(std::unique_ptr<FileReader> reader);

    void ReadBytes(size_t beg, std::span<char> buf) override;
    size_t GetSize() override;
};

};  // namespace curse
