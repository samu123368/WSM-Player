#pragma once
#include "Layout.h"
#include <vector>
#include <string>

// EULA chrome from NAND only. The historical online contract is not in NAND;
// never substitute screenshots or enable acceptance of missing contract text.
class AgreementViewer {
public:
 ~AgreementViewer();
 bool Open(const char16 *no);
 void Render(Mtx &view, const Vec2f &screen, u8 alpha);
 int Count() const { return count; }
private:
 void Clear();
 Layout *layout = NULL;
 std::vector<u8> archiveBytes;
 std::basic_string<char16> noText;
 int count = 0;
};
