#include "rh_unordered_map.h"
#include <iostream>
#include <vector>

inline auto
create_dummy_data()
{
  std::vector data{
    std::make_pair("Hello, World!", 1),
    std::make_pair("foo", 2),
    std::make_pair("bar", 4),
    std::make_pair("spam", 5),
    std::make_pair("bacon", 6),
    std::make_pair("eggs", 7),
    std::make_pair("Goodbye, World!", 8),
    std::make_pair("Modern C++ is awesome", 9),
    std::make_pair("Modern Effective C++ is awesome", 11),
    std::make_pair("Twitch TV", 13),
    std::make_pair("Afreeca TV", 15),
    std::make_pair("Youtube", 20),
  };
  return data;
}

int
main(int argc, const char* argv[])
{
  std::cout << "Hello, world!\n";
  cppref::unordered_map cache;
  auto data = create_dummy_data();
  for (const auto& d : data) {
    cache.insert(d);
  }

  std::cout << "foo, 2 == " << cache.find("foo").value() << '\n';
  std::cout << "Afreeca TV, 15 == " << cache.find("Afreeca TV").value() << '\n';
  std::cout << "Youtube TV, 20 ==" << cache.find("Youtube").value() << '\n';

  return 0;
}
