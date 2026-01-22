
#include <incfontdisc/incfontdisc.hpp>

int
main(int argc, char *argv[]) {

    auto queryFor = incfontdisc::FontQuery{.family = "Arial", .style = "Regular"};
    auto queryFor2 = incfontdisc::FontQuery{.family = "Iosevka Nerd Font", .style = "Regular"};

    // auto rrr = incfontdisc::match_fonts(queryFor);
    auto rrr2 = incfontdisc::match_fonts(queryFor2);

    return 0;
}