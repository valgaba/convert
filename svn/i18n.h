#pragma once
#include <QString>

namespace i18n {
void initialize();
QString language();
QString text(const char *source);
}
