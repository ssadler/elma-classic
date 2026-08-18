
#include "renderer/opengl_widget.h"
#include "renderer/opengl.h"
#include <format>


gl_lifecycle<const std::vector<info_panel_row>&> GlConsole = {
    .render = [](const std::vector<info_panel_row>& info_rows) {

        using namespace std::string_literals;


        std::string spec = "S 14 C \0\0\0\255 "s;
        for (auto r = info_rows.rbegin(); r != info_rows.rend(); r++) {
            auto args = std::make_format_args(r->label, r->value);
            spec += std::vformat("P {}      {:>4}\0 R "s, args);
        }
        spec += "E";

        UIWidget widget = UIWidget(-10, -8, spec).font_size(12);

        gl_render_widget(widget);
    }
};
