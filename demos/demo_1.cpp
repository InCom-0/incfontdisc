
#include <incfontdisc/incfontdisc.hpp>

int
main(int argc, char *argv[]) {

    auto queryFor = incfontdisc::FontQuery{.family = "Arial", .style = "Regular"};

    auto rrr = incfontdisc::match_fonts(queryFor);

    return 0;
}