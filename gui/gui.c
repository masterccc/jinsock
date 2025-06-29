#include <gtk/gtk.h>

typedef struct {
    GtkWidget *entry_filter;
    GtkWidget *treeview;
    GtkWidget *info_label;
    GtkWidget *status_label;
    GtkWidget *progress_bar;
    GtkTreeModel *base_model;
    GtkTreeModelFilter *filter;
    char *filter_query;  // Stocke la chaîne de recherche en minuscules
} AppWidgets;

GtkTreeModel* create_base_model() {
    GtkListStore *store;
    GtkTreeIter iter;

    store = gtk_list_store_new(6,
        G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING,
        G_TYPE_UINT, G_TYPE_STRING, G_TYPE_UINT);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
        0, 1,
        1, 1234,
        2, "bash",
        3, 4,
        4, "127.0.0.1",
        5, 8080,
        -1);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
        0, 2,
        1, 2345,
        2, "firefox",
        3, 5,
        4, "192.168.1.1",
        5, 443,
        -1);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
        0, 3,
        1, 3456,
        2, "python",
        3, 6,
        4, "10.0.0.2",
        5, 9000,
        -1);

    return GTK_TREE_MODEL(store);
}

gboolean filter_visible(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    const gchar *query = app->filter_query;

    if (query == NULL || *query == '\0')
        return TRUE;

    for (int i = 0; i < 6; ++i) {
        GValue val = G_VALUE_INIT;
        gtk_tree_model_get_value(model, iter, i, &val);
        gchar *cell_text = g_strdup_value_contents(&val);
        gchar *cell_lower = g_ascii_strdown(cell_text, -1);

        gboolean match = g_strrstr(cell_lower, query) != NULL;

        g_free(cell_text);
        g_free(cell_lower);
        g_value_unset(&val);

        if (match)
            return TRUE;
    }

    return FALSE;
}

GtkWidget* create_foldable_list() {
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *listbox = gtk_list_box_new();

    for (int i = 0; i < 10; i++) {
        gchar *label = g_strdup_printf("Élément %d", i + 1);
        GtkWidget *expander = gtk_expander_new(label);
        GtkWidget *content = gtk_label_new("Détails ici...");
        gtk_expander_set_child(GTK_EXPANDER(expander), content);
        gtk_list_box_append(GTK_LIST_BOX(listbox), expander);
        g_free(label);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), listbox);
    gtk_widget_set_vexpand(scrolled, TRUE);
    return scrolled;
}

void on_row_selected(GtkTreeSelection *selection, gpointer user_data) {
    AppWidgets *app = (AppWidgets*)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *procname;
        gtk_tree_model_get(model, &iter, 2, &procname, -1);
        gchar *info = g_strdup_printf("Sélectionné : %s", procname);
        gtk_label_set_text(GTK_LABEL(app->info_label), info);
        g_free(info);
        g_free(procname);
    }
}

void on_filter_changed(GtkEditable *editable, gpointer user_data) {
    AppWidgets *app = (AppWidgets *)user_data;

    const gchar *text = gtk_editable_get_text(editable);

    g_free(app->filter_query);
    app->filter_query = g_ascii_strdown(text, -1);

    gtk_tree_model_filter_refilter(app->filter);
}

void on_send_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *app = (AppWidgets*)user_data;
    gtk_label_set_text(GTK_LABEL(app->status_label), "42 bytes sent.");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progress_bar), 0.42);
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppWidgets *widgets = g_new0(AppWidgets, 1);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Interface GTK 4");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    // Marges tout autour
    gtk_widget_set_margin_top(main_box, 5);
    gtk_widget_set_margin_bottom(main_box, 5);
    gtk_widget_set_margin_start(main_box, 5);
    gtk_widget_set_margin_end(main_box, 5);

    gtk_window_set_child(GTK_WINDOW(window), main_box);

    // Titre "Socket list"
    GtkWidget *label_sockets = gtk_label_new("<b>Socket list</b>");
    gtk_label_set_use_markup(GTK_LABEL(label_sockets), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label_sockets), 0.0);
    gtk_box_append(GTK_BOX(main_box), label_sockets);

    // Haut : filtre + tableau
    GtkWidget *top_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    GtkWidget *filter_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    widgets->entry_filter = gtk_entry_new();
    GtkWidget *filter_button = gtk_button_new_with_label("Refresh");
    gtk_box_append(GTK_BOX(filter_box), widgets->entry_filter);
    gtk_box_append(GTK_BOX(filter_box), filter_button);
    gtk_box_append(GTK_BOX(top_box), filter_box);

    g_signal_connect(widgets->entry_filter, "changed", G_CALLBACK(on_filter_changed), widgets);

    widgets->base_model = create_base_model();
    widgets->filter_query = NULL;
    widgets->filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(widgets->base_model, NULL));
    gtk_tree_model_filter_set_visible_func(widgets->filter, filter_visible, widgets, NULL);

    widgets->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(widgets->filter));

    const gchar *titles[] = { "Index", "PID", "ProcName", "FD", "IP", "Port" };
    for (int i = 0; i < 6; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
            titles[i], renderer, "text", i, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(widgets->treeview), column);
    }

    GtkWidget *table_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(table_scroll), widgets->treeview);
    gtk_widget_set_vexpand(table_scroll, TRUE);
    gtk_box_append(GTK_BOX(top_box), table_scroll);

    gtk_box_append(GTK_BOX(main_box), top_box);

    // Titre "Incoming packets"
    GtkWidget *label_packets = gtk_label_new("<b>Incoming packets</b>");
    gtk_label_set_use_markup(GTK_LABEL(label_packets), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label_packets), 0.0);
    gtk_box_append(GTK_BOX(main_box), label_packets);

    // Milieu : liste dépliable
    GtkWidget *middle = create_foldable_list();
    gtk_box_append(GTK_BOX(main_box), middle);

    // Titre "Send data"
    GtkWidget *label_send = gtk_label_new("<b>Send data</b>");
    gtk_label_set_use_markup(GTK_LABEL(label_send), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label_send), 0.0);
    gtk_box_append(GTK_BOX(main_box), label_send);

    // Bas : envoi
    GtkWidget *bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    widgets->info_label = gtk_label_new("Aucune sélection");
    GtkWidget *file_button = gtk_button_new_with_label("Choisir un fichier");
    GtkWidget *text_entry = gtk_entry_new();
    GtkWidget *send_button = gtk_button_new_with_label("Envoyer");

    gtk_widget_set_hexpand(text_entry, TRUE);

    gtk_box_append(GTK_BOX(bottom_box), widgets->info_label);
    gtk_box_append(GTK_BOX(bottom_box), file_button);
    gtk_box_append(GTK_BOX(bottom_box), text_entry);
    gtk_box_append(GTK_BOX(bottom_box), send_button);
    gtk_box_append(GTK_BOX(main_box), bottom_box);

    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), widgets);

    // Statut en bas
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    widgets->status_label = gtk_label_new("0 bytes sent.");
    widgets->progress_bar = gtk_progress_bar_new();

    gtk_widget_set_hexpand(widgets->progress_bar, TRUE);
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(widgets->progress_bar), FALSE);

    gtk_box_append(GTK_BOX(status_bar), widgets->status_label);
    gtk_box_append(GTK_BOX(status_bar), widgets->progress_bar);

    gtk_box_append(GTK_BOX(main_box), status_bar);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->treeview));
    g_signal_connect(selection, "changed", G_CALLBACK(on_row_selected), widgets);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.example.GtkInterface", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
