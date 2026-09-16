#include <string>
#include <utf8.h>

int main()
{
  std::string text = "hello";
  auto        it   = text.begin();

  utf8::next(it, text.end());

  return 0;
}
