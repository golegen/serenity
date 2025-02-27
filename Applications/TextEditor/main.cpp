#include "TextEditorWidget.h"
#include <LibDraw/PNGLoader.h>

int main(int argc, char** argv)
{
    GApplication app(argc, argv);

    auto* window = new GWindow;
    window->set_title("Text Editor");
    window->set_rect(20, 200, 640, 400);

    auto* text_widget = new TextEditorWidget();
    window->set_main_widget(text_widget);

    if (argc >= 2)
        text_widget->open_sesame(argv[1]);

    window->show();
    window->set_icon(load_png("/res/icons/TextEditor16.png"));

    return app.exec();
}
