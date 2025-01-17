//
// Created by jmanc3 on 10/18/23.
//

#include <pango/pangocairo.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include "settings_menu.h"
#include "main.h"
#include "config.h"
#include "components.h"
#include "utility.h"
#include "simple_dbus.h"

WinbarSettings *winbar_settings = new WinbarSettings;

void merge_order_with_taskbar();

static void
paint_root(AppClient *client, cairo_t *cr, Container *container) {
    set_rect(cr, container->real_bounds);
    set_argb(cr, correct_opaqueness(client, config->color_pinned_icon_editor_background));
    cairo_fill(cr);
}

static void paint_label(AppClient *client, cairo_t *cr, Container *container) {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    auto label = (Label *) container->user_data;
    int size = label->size;
    if (size == -1)
        size = 9 * config->dpi;
    PangoLayout *layout = get_cached_pango_font(cr, config->font, size, label->weight);
    
    int width;
    int height;
    pango_layout_set_text(layout, label->text.c_str(), -1);
    pango_layout_get_pixel_size(layout, &width, &height);
    if (label->color.a == 0) {
        set_argb(cr, config->color_pinned_icon_editor_field_default_text);
    } else {
        set_argb(cr, label->color);
    }
    cairo_move_to(cr, container->real_bounds.x + container->wanted_pad.x,
                  container->real_bounds.y + container->real_bounds.h / 2 - height / 2);
    pango_cairo_show_layout(cr, layout);
}

static void paint_draggable(AppClient *client, cairo_t *cr, Container *container) {
    float dot_size = 3 * config->dpi;
    float height = dot_size * 5; // three real and 2 spaces between
    float start_x = container->real_bounds.x + container->real_bounds.w / 2 - dot_size / 2 - dot_size;
    float start_y = container->real_bounds.y + container->real_bounds.h / 2 - height / 2;
    set_argb(cr, config->color_pinned_icon_editor_field_default_border);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 2; ++x) {
            set_rect(cr, Bounds(start_x + (dot_size * x) + (dot_size * x),
                                start_y + (dot_size * y) + (dot_size * y),
                                dot_size,
                                dot_size));
        }
    }
    cairo_fill(cr);
}

struct Checkbox : UserData {
    bool on = true;
    std::string container_name;
    std::string name;
};

static void clicked_on_off(AppClient *client, cairo_t *cr, Container *container) {
    auto *data = (Checkbox *) container->user_data;
    data->on = !data->on;
    
    for (auto &c: winbar_settings->taskbar_order) {
        if (c.name == data->name) {
            c.on = data->on;
            break;
        }
    }
    merge_order_with_taskbar();
}

static void paint_on_off(AppClient *client, cairo_t *cr, Container *container) {
    auto *data = (Checkbox *) container->user_data;
    
    float size = 14 * config->dpi;
    rounded_rect(cr, 2 * config->dpi,
                 container->real_bounds.x + container->real_bounds.w / 2 - size / 2,
                 container->real_bounds.y + container->real_bounds.h / 2 - size / 2,
                 size, size);
    set_argb(cr, config->color_pinned_icon_editor_field_default_border);
    if (container->state.mouse_hovering)
        set_argb(cr, config->color_pinned_icon_editor_field_hovered_border);
    if (container->state.mouse_pressing)
        set_argb(cr, config->color_pinned_icon_editor_field_pressed_border);
    
    if (data->on)
        set_argb(cr, config->color_pinned_icon_editor_field_pressed_border);
    
    if (data->on) {
        cairo_fill(cr);
    } else {
        cairo_set_line_width(cr, std::round(1 * config->dpi));
        cairo_stroke(cr);
    }
    
    if (!data->on)
        return;
    
    PangoLayout *layout =
            get_cached_pango_font(cr, "Segoe MDL2 Assets Mod", 14 * config->dpi, PangoWeight::PANGO_WEIGHT_NORMAL);
    
    // from https://docs.microsoft.com/en-us/windows/apps/design/style/segoe-ui-symbol-font
    pango_layout_set_text(layout, "\uF13E", strlen("\uE83F"));
    
    set_argb(cr, ArgbColor(1, 1, 1, 1));
    
    int width;
    int height;
    pango_layout_get_pixel_size(layout, &width, &height);
    
    cairo_move_to(cr,
                  (int) (container->real_bounds.x + container->real_bounds.w / 2 - width / 2),
                  (int) (container->real_bounds.y + container->real_bounds.h / 2 - height / 2 - 1 * config->dpi));
    pango_cairo_show_layout(cr, layout);
}

static void paint_reordable_item(AppClient *client, cairo_t *cr, Container *container) {
    rounded_rect(cr, 4 * config->dpi, container->real_bounds.x, container->real_bounds.y, container->real_bounds.w,
                 container->real_bounds.h);
    set_argb(cr, config->color_pinned_icon_editor_background);
    cairo_fill(cr);
    rounded_rect(cr, 4 * config->dpi, container->real_bounds.x, container->real_bounds.y, container->real_bounds.w,
                 container->real_bounds.h);
    set_argb(cr, config->color_pinned_icon_editor_field_default_border);
    if (container->state.mouse_hovering)
        set_argb(cr, config->color_pinned_icon_editor_field_hovered_border);
    if (container->state.mouse_pressing)
        set_argb(cr, config->color_pinned_icon_editor_field_pressed_border);
    cairo_set_line_width(cr, std::round(1 * config->dpi));
    cairo_stroke(cr);
}

static void paint_remove(AppClient *client, cairo_t *cr, Container *container) {
    // check: F13E
    PangoLayout *layout =
            get_cached_pango_font(cr, "Segoe MDL2 Assets Mod", 10 * config->dpi, PangoWeight::PANGO_WEIGHT_NORMAL);
    
    // from https://docs.microsoft.com/en-us/windows/apps/design/style/segoe-ui-symbol-font
    pango_layout_set_text(layout, "\uE107", strlen("\uE83F"));
    
    set_argb(cr, ArgbColor(.8, .3, .1, 1));
    
    int width;
    int height;
    pango_layout_get_pixel_size(layout, &width, &height);
    
    cairo_move_to(cr,
                  (int) (container->real_bounds.x + container->real_bounds.w / 2 - width / 2),
                  (int) (container->real_bounds.y + container->real_bounds.h / 2 - height / 2));
    pango_cairo_show_layout(cr, layout);
}

static void clicked_remove_reorderable(AppClient *client, cairo_t *cr, Container *container) {
    auto containers = container->parent->parent->children;
    auto it = std::find(containers.begin(), containers.end(), container->parent);
    
    if (it != containers.end()) {
        size_t current_index = std::distance(containers.begin(), it);
        
        container->parent->parent->children.erase(container->parent->parent->children.begin() + current_index);
        client_layout(app, client);
    }
}

struct Drag : UserData {
    int initial_mouse_click_before_drag_offset_y = 0;
};

static void dragged_list_start(AppClient *client, cairo_t *cr, Container *container) {
    auto *data = (Drag *) container->user_data;
    data->initial_mouse_click_before_drag_offset_y = container->real_bounds.y - client->mouse_initial_y;
    for (auto c: container->parent->children)
        c->z_index = 0;
    container->z_index = 20;
}

void move_container_to_index(std::vector<Container *> &containers, Container *container, int wants_i) {
    auto it = std::find(containers.begin(), containers.end(), container);
    
    if (it != containers.end()) {
        size_t current_index = std::distance(containers.begin(), it);
        
        if (current_index != static_cast<size_t>(wants_i)) {
            if (current_index < wants_i) {
                // Move forward in the vector.
                for (size_t i = current_index; i < wants_i; ++i) {
                    containers[i] = containers[i + 1];
                }
            } else {
                // Move backward in the vector.
                for (size_t i = current_index; i > static_cast<size_t>(wants_i); --i) {
                    containers[i] = containers[i - 1];
                }
            }
            
            // Finally, place 'container' at the new index 'wants_i'.
            containers[wants_i] = container;
        }
    }
}

static void dragged_list_item(AppClient *client, cairo_t *cr, Container *container) {
    auto *data = (Drag *) container->user_data;
    
    // parent layout
    layout(client, cr, container->parent, container->parent->real_bounds);
    double max_y = container->parent->children[container->parent->children.size() - 1]->real_bounds.y;
    
    float drag_position_y = client->mouse_current_y + data->initial_mouse_click_before_drag_offset_y;
    bool wants_different = false;
    int wants_i;
    for (int i = 0; i < container->parent->children.size(); i++) {
        auto c = container->parent->children[i];
        if (c == container) // don't replace self
            continue;
        float half_height = c->real_bounds.h / 2;
        float c_middle = c->real_bounds.y + half_height;
        if (std::abs(c_middle - (drag_position_y + half_height)) < half_height) {
            wants_different = true;
            wants_i = i;
            break;
        }
    }
    if (wants_different) {
        move_container_to_index(container->parent->children, container, wants_i);
        layout(client, cr, container->parent, container->parent->real_bounds);
    }
    
    // put drag item where it is
    container->real_bounds.y = drag_position_y;
    
    double min_y = container->parent->real_bounds.y;
    if (container->real_bounds.y < min_y)
        container->real_bounds.y = min_y;
    if (container->real_bounds.y > max_y)
        container->real_bounds.y = max_y;
    
    // internal layout
    layout(client, cr, container, container->real_bounds);
}

static void dragged_list_item_end(AppClient *client, cairo_t *cr, Container *container) {
    layout(client, cr, container->parent, container->parent->real_bounds);
    
    Container *reorder_list = container_by_name("reorder_list", client->root);
    std::sort(winbar_settings->taskbar_order.begin(), winbar_settings->taskbar_order.end(),
              [reorder_list](const TaskbarItem &first, const TaskbarItem &second) {
                  int first_index = 1000;
                  int second_index = 1000;
                  for (int i = 0; i < reorder_list->children.size(); ++i) {
                    auto *label = (Label *) reorder_list->children[i]->children[2]->user_data;
                    if (label->text == first.name) {
                        first_index = i;
                        break;
                    }
                  }
                  for (int i = 0; i < reorder_list->children.size(); ++i) {
                      auto *label = (Label *) reorder_list->children[i]->children[2]->user_data;
                      if (label->text == second.name) {
                          second_index = i;
                          break;
                      }
                  }
                  return first_index < second_index;
              });
    for (int i = 0; i < winbar_settings->taskbar_order.size(); i++)
        winbar_settings->taskbar_order[i].target_index = i;
    merge_order_with_taskbar();
}

static void add_item(Container *reorder_list, std::string n, bool on_off_state) {
    auto r = reorder_list->child(::hbox, FILL_SPACE, 28 * config->dpi);
    r->when_paint = paint_reordable_item;
    r->receive_events_even_if_obstructed_by_one = true;
    r->clip = true;
    r->when_drag_start = dragged_list_start;
    r->when_drag = dragged_list_item;
    r->when_drag_end = dragged_list_item_end;
    r->when_drag_end_is_click = false;
    r->user_data = new Drag;
    
    auto drag = r->child(r->wanted_bounds.h * .8, FILL_SPACE);
    drag->wanted_pad.x = r->wanted_bounds.h * .43;
    drag->when_paint = paint_draggable;
    
    if (n == "Space") {
        auto remove = r->child(r->wanted_bounds.h * .8, FILL_SPACE);
        remove->wanted_pad.x = r->wanted_bounds.h * .43;
        remove->when_clicked = clicked_remove_reorderable;
        remove->when_paint = paint_remove;
    } else {
        auto on_off = r->child(r->wanted_bounds.h * .8, FILL_SPACE);
        on_off->wanted_pad.x = r->wanted_bounds.h * .43;
        on_off->when_clicked = clicked_on_off;
        on_off->when_paint = paint_on_off;
        auto check = new Checkbox;
        check->name = n;
        check->on = on_off_state;
        on_off->user_data = check;
    }
    
    auto x = r->child(::hbox, FILL_SPACE, FILL_SPACE);
    x->wanted_pad.x = r->wanted_bounds.h * .4;
    x->when_paint = paint_label;
    auto data = new Label(n);
    x->user_data = data;
}

static void clicked_add_spacer(AppClient *client, cairo_t *cr, Container *container) {
    add_item(container_by_name("reorder_list", client->root), "Space", true);
    client_layout(app, client);
}

static void paint_centered_text(AppClient *client, cairo_t *cr, Container *container) {
    auto *label = (Label *) container->user_data;
    paint_reordable_item(client, cr, container);
    PangoLayout *layout = get_cached_pango_font(cr, config->font, 9 * config->dpi, PANGO_WEIGHT_NORMAL);
    
    int width;
    int height;
    pango_layout_set_text(layout, label->text.c_str(), -1);
    pango_layout_get_pixel_size(layout, &width, &height);
    set_argb(cr, config->color_pinned_icon_editor_field_default_text);
    cairo_move_to(cr, container->real_bounds.x + container->real_bounds.w / 2 - width / 2,
                  container->real_bounds.y + container->real_bounds.h / 2 - height / 2);
    pango_cairo_show_layout(cr, layout);
}

void merge_order_with_taskbar() {
    for (const auto &item: winbar_settings->taskbar_order) {
        if (item.name == "Bluetooth") {
            winbar_settings->bluetooth_enabled = item.on;
        }
    }
    std::sort(winbar_settings->taskbar_order.begin(), winbar_settings->taskbar_order.end(),
              [](const TaskbarItem &first, const TaskbarItem &second) {
                  return first.target_index < second.target_index;
              });
    
    auto taskbar = client_by_name(app, "taskbar");
    if (!taskbar)
        return;

#define ADD(button_name, container_name) if (s.name == button_name) { \
       auto container = container_by_name(container_name, taskbar->root); \
       container->exists = s.on; \
       containers.push_back(container); \
       continue; \
    }
    
    std::vector<Container *> containers;
    for (const auto &s: winbar_settings->taskbar_order) {
        ADD("Super", "super")
        ADD("Search Field", "field_search")
        ADD("Workspace", "workspace")
        ADD("Pinned Icons", "icons")
        ADD("Systray", "systray")
        ADD("Bluetooth", "bluetooth")
        ADD("Wifi", "wifi")
        ADD("Battery", "battery")
        ADD("Volume", "volume")
        ADD("Date", "date")
        ADD("Notifications", "action")
        ADD("Show Desktop", "minimize")
    }
    taskbar->root->children.clear();
    for (auto c: containers) {
        taskbar->root->children.push_back(c);
    }
    // No matter what, bluetooth does not exist until dbus says it does.
    container_by_name("bluetooth", taskbar->root)->exists = false;
    
    client_layout(app, taskbar);
    client_paint(app, taskbar);
}

static void clicked_reset(AppClient *client, cairo_t *, Container *) {
    auto reorder_list = container_by_name("reorder_list", client->root);
    for (auto c: reorder_list->children)
        delete c;
    reorder_list->children.clear();
    std::vector<std::string> names = {"Super", "Search Field", "Workspace", "Pinned Icons",
                                      "Systray", "Bluetooth", "Battery", "Wifi",
                                      "Volume", "Date", "Notifications", "Show Desktop"};
    for (auto n: names) {
        add_item(reorder_list, n, true);
    }
    winbar_settings->taskbar_order.clear();
    for (int i = 0; i < names.size(); ++i) {
        TaskbarItem item;
        item.name = names[i];
        item.on = true;
        item.target_index = i;
        winbar_settings->taskbar_order.push_back(item);
    }
    client_layout(app, client);
    merge_order_with_taskbar();
    save_settings_file();
}

static void
fill_root(AppClient *client, Container *root) {
    root->when_paint = paint_root;
    root->type = ::vbox;
    root->wanted_pad = Bounds(24 * config->dpi, 62 * config->dpi, 24 * config->dpi, 24 * config->dpi);
    
    {
        auto title = root->child(FILL_SPACE, 20 * config->dpi);
        auto data = new Label("Taskbar");
        data->size = 20 * config->dpi;
        title->when_paint = paint_label;
        title->user_data = data;
    }
    
    root->child(FILL_SPACE, 32 * config->dpi);
    
    ScrollPaneSettings scroll_settings(config->dpi);
    scroll_settings.right_width = 6 * config->dpi;
    scroll_settings.paint_minimal = true;
    auto scrollpane = make_newscrollpane_as_child(root, scroll_settings);
    scrollpane->content = new Container(::vbox, FILL_SPACE, USE_CHILD_SIZE);
    auto reorder_list = scrollpane->content;
    reorder_list->parent = scrollpane;
    reorder_list->name = "reorder_list";
    reorder_list->wanted_pad.y = std::round(1 * config->dpi);
    reorder_list->wanted_pad.x = std::round(1 * config->dpi);
    reorder_list->wanted_pad.w = std::round(1 * config->dpi);
    reorder_list->wanted_pad.h = std::round(1 * config->dpi);
    reorder_list->spacing = 4 * config->dpi;
    for (auto item: winbar_settings->taskbar_order) {
        if (item.name == "Bluetooth") {
            add_item(reorder_list, item.name, winbar_settings->bluetooth_enabled);
        } else {
            add_item(reorder_list, item.name, item.on);
        }
    }
    
    root->child(FILL_SPACE, 16 * config->dpi); // space after re-orderable list
    
    {
//        auto x = root->child(::hbox, FILL_SPACE, 28 * config->dpi);
//        x->when_paint = paint_centered_text;
//        x->when_clicked = clicked_add_spacer;
//        x->user_data = new Label("Add spacer");
//        root->child(FILL_SPACE, 6 * config->dpi);
    }
    
    {
        auto x = root->child(::hbox, FILL_SPACE, 28 * config->dpi);
        x->when_paint = paint_centered_text;
        x->user_data = new Label("Reset to default");
        x->when_clicked = clicked_reset;
    }
}

// Trim leading and trailing whitespace from a string in-place
void trim(std::string &str) {
    // Remove leading whitespace
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int ch) {
        return !std::isspace(ch);
    }));
    
    // Remove trailing whitespace
    str.erase(std::find_if(str.rbegin(), str.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

void when_closed_settings_menu(AppClient *client) {
    save_settings_file();
}

void open_settings_menu(SettingsPage page) {
    Settings settings;
    settings.skip_taskbar = false;
//    settings.keep_above = true;
    settings.w = 1000 * config->dpi;
    settings.h = 700 * config->dpi;
    auto client = client_new(app, settings, "settings_menu");
    client->when_closed = when_closed_settings_menu;
    xcb_ewmh_set_wm_icon_name(&app->ewmh, client->window, strlen("settings"), "settings");
    std::string title = "Taskbar Settings";
    xcb_ewmh_set_wm_name(&app->ewmh, client->window, title.length(), title.c_str());
    fill_root(client, client->root);
    
    client_show(app, client);
    xcb_set_input_focus(app->connection, XCB_NONE, client->window, XCB_CURRENT_TIME);
    xcb_flush(app->connection);
    xcb_aux_sync(app->connection);
    active_window_changed(client->window);
}

void save_settings_file() {
    char *home = getenv("HOME");
    std::string path = std::string(home) + "/.config/winbar/settings.conf";
    std::ofstream out_file(path);
    out_file << "order=";

#define WRITE(button_name, container_name) \
    if (child->name == container_name) {   \
       out_file << "\"" << button_name << "\"=" << (child->exists ? "on," : "off,"); \
       continue; \
    }
    
    if (auto c = client_by_name(app, "taskbar")) {
        for (auto child: c->root->children) {
            WRITE("Super", "super")
            WRITE("Search Field", "field_search")
            WRITE("Workspace", "workspace")
            WRITE("Pinned Icons", "icons")
            WRITE("Systray", "systray")
            if (child->name == "bluetooth") {
                out_file << "\"" << "Bluetooth" << "\"=" << (winbar_settings->bluetooth_enabled ? "on," : "off,");
                continue;
            }
            WRITE("Wifi", "wifi")
            WRITE("Battery", "battery")
            WRITE("Volume", "volume")
            WRITE("Date", "date")
            WRITE("Notifications", "action")
            WRITE("Show Desktop", "minimize")
        }
        out_file << std::endl;
    }
    out_file << std::endl;
}

void read_settings_file() {
    winbar_settings->taskbar_order.clear();
    // Load default taskbar_order
    std::vector<std::string> names = {"Super", "Search Field", "Workspace", "Pinned Icons",
                                      "Systray", "Bluetooth", "Battery", "Wifi",
                                      "Volume", "Date", "Notifications", "Show Desktop"};
    for (int i = 0; i < names.size(); ++i) {
        TaskbarItem item;
        item.name = names[i];
        item.on = true;
        item.target_index = i;
        winbar_settings->taskbar_order.push_back(item);
    }
    std::vector<TaskbarItem> file_order;
    bool found_order = false;
    
    char *home = getenv("HOME");
    std::string path = std::string(home) + "/.config/winbar/settings.conf";
    std::ifstream input_file(path);
    if (input_file.good()) {
        std::string line;
        while (std::getline(input_file, line)) {
            LineParser parser(line);
            std::string key = parser.until(LineParser::Token::EQUAL);
            trim(key);
            
            if (key == "order") {
                found_order = true;
#define EAT_EXPECTED(type) if (parser.current_token != type) {goto out;} else{parser.next();};
                std::string first;
                std::string second;
                while (parser.current_token != LineParser::Token::END_OF_LINE) {
                    parser.until(LineParser::Token::QUOTE);
                    EAT_EXPECTED(LineParser::Token::QUOTE)
                    first = parser.until(LineParser::Token::QUOTE);
                    EAT_EXPECTED(LineParser::Token::QUOTE)
                    parser.until(LineParser::Token::EQUAL);
                    EAT_EXPECTED(LineParser::Token::EQUAL)
                    second = parser.until(LineParser::Token::COMMA);
                    trim(second);
                    
                    // Skip adding to file_order if already added unless if it's 'Space'
                    bool should_be_added = true;
                    for (auto item: file_order)
                        if (item.name == first)
                            should_be_added = first == "Space";
                    
                    if (should_be_added) {
                        TaskbarItem item;
                        item.name = first;
                        item.on = second == "on";
                        item.target_index = file_order.size();
                        file_order.push_back(item);
                    }
                }
                out:
                continue;
            }
        }
    }
    
    for (auto &order: winbar_settings->taskbar_order) {
        TaskbarItem *found = nullptr;
        for (auto &file: file_order) {
            if (order.name == file.name) {
                found = &file;
                break;
            }
        }
        if (found) {
            order.on = found->on;
            order.target_index = found->target_index;
        } else {
            order.on = !found_order;
            order.target_index += 1000;
        }
    }
    merge_order_with_taskbar();
}