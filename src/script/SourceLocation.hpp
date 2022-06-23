#ifndef SOURCE_LOCATION_HPP
#define SOURCE_LOCATION_HPP

#include <string>

class SourceLocation {
public:
    static const SourceLocation eof;

public:
    SourceLocation(int line, int column, const std::string &filename);
    SourceLocation(const SourceLocation &other);

    inline int GetLine() const { return m_line; }
    inline int &GetLine() { return m_line; }
    inline int GetColumn() const { return m_column; }
    inline int &GetColumn() { return m_column; }
    inline const std::string &GetFileName() const { return m_filename; }
    inline void SetFileName(const std::string &filename) { m_filename = filename; }

    bool operator<(const SourceLocation &other) const;
    bool operator==(const SourceLocation &other) const;

private:
    int m_line;
    int m_column;
    std::string m_filename;
};

#endif
