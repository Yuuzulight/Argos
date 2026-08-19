#include "skin/IniParser.h"
#include <cassert>
#include <cstdio>

using namespace argos;

int main() {
    // Valid: sections, key=value, comments, blank lines.
    {
        auto result = ParseIni(
            "; comment\n"
            "[Widget]\n"
            "X=40\n"
            "Y=40\n"
            "\n"
            "# also a comment\n"
            "[MeasureClock]\n"
            "Measure=Clock\n");
        assert(result.Ok());
        assert(result.sections.size() == 2);
        assert(result.sections[0].name == "Widget");
        assert(*result.sections[0].Find("X") == "40");
        assert(result.sections[0].Find("Missing") == nullptr);
        assert(result.FindSection("MeasureClock") != nullptr);
        assert(*result.FindSection("MeasureClock")->Find("Measure") == "Clock");
    }

    // Error: key=value before any section header.
    {
        auto result = ParseIni("X=40\n");
        assert(!result.Ok());
        assert(result.error->line == 1);
    }

    // Error: line inside a section that isn't key=value.
    {
        auto result = ParseIni("[Widget]\nnotakeyvalue\n");
        assert(!result.Ok());
        assert(result.error->line == 2);
    }

    // Error: duplicate section name.
    {
        auto result = ParseIni("[Widget]\nX=1\n[Widget]\nY=2\n");
        assert(!result.Ok());
        assert(result.error->line == 3);
    }

    printf("IniParser: all checks passed\n");
    return 0;
}
