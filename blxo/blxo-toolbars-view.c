/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 * Copyright (c) 2003      Marco Pesenti Gritti
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <blxo/blxo-config.h>
#include <blxo/blxo-private.h>
#include <blxo/blxo-string.h>
#include <blxo/blxo-toolbars-editor.h>
#include <blxo/blxo-toolbars-private.h>
#include <blxo/blxo-toolbars-view.h>
#include <blxo/blxo-alias.h>

/**
 * SECTION: blxo-toolbars-view
 * @title: BlxoToolbarsView
 * @short_description: Widget for displaying toolbars
 * @include: blxo/blxo.h
 * @see_also: #BlxoToolbarsEditor, #BlxoToolbarsEditorDialog, #BlxoToolbarsModel
 *
 * A widget that displays toolbars as described in a #BlxoToolbarsModel object.
 **/



#define MIN_TOOLBAR_HEIGHT  20



enum
{
  PROP_0,
  PROP_EDITING,
  PROP_MODEL,
  PROP_UI_MANAGER,
};

enum
{
  ACTION_REQUEST,
  CUSTOMIZE,
  LAST_SIGNAL,
};



static void         blxo_toolbars_view_finalize              (GObject              *object);
static void         blxo_toolbars_view_get_property          (GObject              *object,
                                                             guint                 prop_id,
                                                             GValue               *value,
                                                             GParamSpec           *pspec);
static void         blxo_toolbars_view_set_property          (GObject              *object,
                                                             guint                 prop_id,
                                                             const GValue         *value,
                                                             GParamSpec           *pspec);
static gint         blxo_toolbars_view_get_toolbar_position  (BlxoToolbarsView      *view,
                                                             GtkWidget            *toolbar);
static gint         blxo_toolbars_view_get_n_toolbars        (BlxoToolbarsView      *view);
static GtkWidget   *blxo_toolbars_view_get_dock_nth          (BlxoToolbarsView      *view,
                                                             gint                  position);
static GtkWidget   *blxo_toolbars_view_get_toolbar_nth       (BlxoToolbarsView      *view,
                                                             gint                  position);
static void         blxo_toolbars_view_drag_data_delete      (GtkWidget            *item,
                                                             GdkDragContext       *context,
                                                             BlxoToolbarsView      *view);
static void         blxo_toolbars_view_drag_data_get             (GtkWidget            *item,
                                                                 GdkDragContext       *context,
                                                                 GtkSelectionData     *selection_data,
                                                                 guint                 info,
                                                                 guint32               drag_time,
                                                                 BlxoToolbarsView      *view);
static GtkWidget   *blxo_toolbars_view_create_item_from_action   (BlxoToolbarsView      *view,
                                                                 const gchar          *action_name,
                                                                 const gchar          *type,
                                                                 gboolean              is_separator,
                                                                 GtkAction           **ret_action);
static GtkWidget   *blxo_toolbars_view_create_item               (BlxoToolbarsView      *view,
                                                                 BlxoToolbarsModel     *model,
                                                                 gint                  toolbar_position,
                                                                 gint                  item_position,
                                                                 GtkAction           **ret_action);
static void         blxo_toolbars_view_drag_data_received        (GtkWidget            *toolbar,
                                                                 GdkDragContext       *context,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 GtkSelectionData     *selection_data,
                                                                 guint                 info,
                                                                 guint                 drag_time,
                                                                 BlxoToolbarsView      *view);
static void         blxo_toolbars_view_context_menu              (GtkWidget            *toolbar,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 gint                  button,
                                                                 BlxoToolbarsView      *view);
static void         blxo_toolbars_view_free_dragged_item         (BlxoToolbarsView      *view);
static gboolean     blxo_toolbars_view_drag_drop                 (GtkWidget            *widget,
                                                                 GdkDragContext       *context,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 guint                 drag_time,
                                                                 BlxoToolbarsView      *view);
static gboolean     blxo_toolbars_view_drag_motion               (GtkWidget            *toolbar,
                                                                 GdkDragContext       *context,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 guint                 drag_time,
                                                                 BlxoToolbarsView      *view);
static void         blxo_toolbars_view_drag_leave                (GtkWidget            *toolbar,
                                                                 GdkDragContext       *context,
                                                                 guint                 drag_time,
                                                                 BlxoToolbarsView      *view);
static GtkWidget   *blxo_toolbars_view_create_dock               (BlxoToolbarsView      *view);
static void         blxo_toolbars_view_toolbar_added             (BlxoToolbarsModel     *model,
                                                                 gint                  position,
                                                                 BlxoToolbarsView      *view);
static void         blxo_toolbars_view_toolbar_changed           (BlxoToolbarsModel     *model,
                                                                 gint                  position,
                                                                 BlxoToolbarsView      *view);
static void         blxo_toolbars_view_toolbar_removed           (BlxoToolbarsModel     *model,
                                                                 gint                  position,
                                                                 BlxoToolbarsView      *view);
static void         blxo_toolbars_view_item_added                (BlxoToolbarsModel     *model,
                                                                 gint                  toolbar_position,
                                                                 gint                  item_position,
                                                                 BlxoToolbarsView      *view);
static void         blxo_toolbars_view_item_removed              (BlxoToolbarsModel     *model,
                                                                 gint                  toolbar_position,
                                                                 gint                  item_position,
                                                                 BlxoToolbarsView      *view);
static void         blxo_toolbars_view_construct                 (BlxoToolbarsView      *view);




struct _BlxoToolbarsViewPrivate
{
  gboolean          editing;
  BlxoToolbarsModel *model;
  GtkUIManager     *ui_manager;

  GtkWidget        *selected_toolbar;
  GtkWidget        *target_toolbar;
  GtkWidget        *dragged_item;

  guint             pending : 1;
};



static const GtkTargetEntry dst_targets[] =
{
  { BLXO_TOOLBARS_ITEM_TYPE, GTK_TARGET_SAME_APP, 0 },
};

static guint  toolbars_view_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_PRIVATE (BlxoToolbarsView, blxo_toolbars_view, GTK_TYPE_VBOX)



static void
blxo_toolbars_view_class_init (BlxoToolbarsViewClass *klass)
{
  GObjectClass *gobject_class;

  /* initialize blxo i18n support */
  _blxo_i18n_init ();

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = blxo_toolbars_view_finalize;
  gobject_class->get_property = blxo_toolbars_view_get_property;
  gobject_class->set_property = blxo_toolbars_view_set_property;

  /**
   * BlxoToolbarsView:editing:
   *
   * This property tells if the toolbars contained with this
   * #BlxoToolbarsView are currently being edited by the user.
   * If the user edits a view, the view will act as proxy
   * and make the requested changes to the model.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_EDITING,
                                   g_param_spec_boolean ("editing",
                                                         "Editing",
                                                         "Editing",
                                                         FALSE,
                                                         BLXO_PARAM_READWRITE));

  /**
   * BlxoToolbarsView:model:
   *
   * The #BlxoToolbarsModel associated with this #BlxoToolbarsView
   * or %NULL if there is no model currently associated with this
   * view. The view is build up from the model, which says, that
   * it will display the toolbars as described in the model.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        "Model",
                                                        "Model",
                                                        BLXO_TYPE_TOOLBARS_MODEL,
                                                        BLXO_PARAM_READWRITE));

  /**
   * BlxoToolbarsView:ui-manager:
   *
   * The #GtkUIManager currently associated with this #BlxoToolbarsView
   * or %NULL. The #GtkUIManager object is used to translate action
   * names as used by the #BlxoToolbarsModel into #GtkAction objects,
   * which are then used to create and maintain the items in the
   * toolbars.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_UI_MANAGER,
                                   g_param_spec_object ("ui-manager",
                                                        "UI Manager",
                                                        "UI Manager",
                                                        GTK_TYPE_UI_MANAGER,
                                                        BLXO_PARAM_READWRITE));

  /**
   * BlxoToolbarsView::action-request:
   * @view  : An #BlxoToolbarsView.
   *
   * This signal is emitted when a new #GtkToolItem is created from a
   * #GtkAction in the #BlxoToolbarsView.
   **/
  toolbars_view_signals[ACTION_REQUEST] =
    g_signal_new (I_("action-request"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (BlxoToolbarsViewClass, action_request),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  /**
   * BlxoToolbarsView::customize:
   * @view  : An #BlxoToolbarsView.
   *
   * This signal is emitted if the users chooses the
   * <emphasis>Customize Toolbars...</emphasis> option
   * from the right-click menu.
   *
   * Please take note, that the option will only be
   * present in the right-click menu, if you had previously
   * connected a handler to this signal.
   **/
  toolbars_view_signals[CUSTOMIZE] =
    g_signal_new (I_("customize"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (BlxoToolbarsViewClass, customize),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
blxo_toolbars_view_init (BlxoToolbarsView *view)
{
  view->priv = blxo_toolbars_view_get_instance_private (view);
}



static void
blxo_toolbars_view_finalize (GObject *object)
{
  BlxoToolbarsView *view = BLXO_TOOLBARS_VIEW (object);

  blxo_toolbars_view_set_model (view, NULL);
  blxo_toolbars_view_set_ui_manager (view, NULL);

  (*G_OBJECT_CLASS (blxo_toolbars_view_parent_class)->finalize) (object);
}



static void
blxo_toolbars_view_get_property (GObject              *object,
                                guint                 prop_id,
                                GValue               *value,
                                GParamSpec           *pspec)
{
  BlxoToolbarsView *view = BLXO_TOOLBARS_VIEW (object);

  switch (prop_id)
    {
    case PROP_EDITING:
      g_value_set_boolean (value, view->priv->editing);
      break;

    case PROP_MODEL:
      g_value_set_object (value, view->priv->model);
      break;

    case PROP_UI_MANAGER:
      g_value_set_object (value, view->priv->ui_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
blxo_toolbars_view_set_property (GObject              *object,
                                guint                 prop_id,
                                const GValue         *value,
                                GParamSpec           *pspec)
{
  BlxoToolbarsView *view = BLXO_TOOLBARS_VIEW (object);

  switch (prop_id)
    {
    case PROP_EDITING:
      blxo_toolbars_view_set_editing (view, g_value_get_boolean (value));
      break;

    case PROP_MODEL:
      blxo_toolbars_view_set_model (view, g_value_get_object (value));
      break;

    case PROP_UI_MANAGER:
      blxo_toolbars_view_set_ui_manager (view, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gint
blxo_toolbars_view_get_toolbar_position (BlxoToolbarsView *view,
                                        GtkWidget       *toolbar)
{
  GList *children;
  gint   position;

  children = gtk_container_get_children (GTK_CONTAINER (view));
  position = g_list_index (children, gtk_widget_get_parent (toolbar));
  g_list_free (children);

  return position;
}



static gint
blxo_toolbars_view_get_n_toolbars (BlxoToolbarsView *view)
{
  GList *children;
  gint   length;

  children = gtk_container_get_children (GTK_CONTAINER (view));
  length = g_list_length (children);
  g_list_free (children);

  return length;
}



static GtkWidget*
blxo_toolbars_view_get_dock_nth (BlxoToolbarsView *view,
                                gint             position)
{
  GtkWidget *dock;
  GList     *children;

  children = gtk_container_get_children (GTK_CONTAINER (view));
  dock = g_list_nth_data (children, position);
  g_list_free (children);

  return dock;
}



static GtkWidget*
blxo_toolbars_view_get_toolbar_nth (BlxoToolbarsView *view,
                                   gint             position)
{
  GtkWidget *toolbar;
  GtkWidget *dock;
  GList     *children;

  dock = blxo_toolbars_view_get_dock_nth (view, position);
  if (G_LIKELY (dock != NULL))
    {
      children = gtk_container_get_children (GTK_CONTAINER (dock));
      toolbar = GTK_WIDGET (children->data);
      g_list_free (children);
    }
  else
    {
      toolbar = NULL;
    }

  return toolbar;
}



static void
blxo_toolbars_view_drag_data_delete (GtkWidget       *item,
                                    GdkDragContext  *context,
                                    BlxoToolbarsView *view)
{
  gint toolbar_position;
  gint item_position;

  item_position = gtk_toolbar_get_item_index (GTK_TOOLBAR (gtk_widget_get_parent (item)),
                                              GTK_TOOL_ITEM (item));
  toolbar_position = blxo_toolbars_view_get_toolbar_position (view, gtk_widget_get_parent (item));
  blxo_toolbars_model_remove_item (view->priv->model, toolbar_position, item_position);
}



static void
blxo_toolbars_view_drag_data_get (GtkWidget        *item,
                                 GdkDragContext   *context,
                                 GtkSelectionData *selection_data,
                                 guint             info,
                                 guint32           drag_time,
                                 BlxoToolbarsView  *view)
{
  const gchar *type;
  const gchar *id;
  gchar       *target;

  type = g_object_get_data (G_OBJECT (item), I_("type"));
  id = g_object_get_data (G_OBJECT (item), I_("id"));

  if (blxo_str_is_equal (id, "separator"))
    target = g_strdup (id);
  else
    target = blxo_toolbars_model_get_item_data (view->priv->model, type, id);

  gtk_selection_data_set (selection_data, gtk_selection_data_get_target(selection_data),
                          8, (guchar *) target, strlen (target));

  g_free (target);
}



static void
set_item_drag_source (BlxoToolbarsModel  *model,
                      GtkWidget         *item,
                      GtkAction         *action,
                      gboolean           is_separator,
                      const gchar       *type)
{
  GtkTargetEntry target_entry;
  const gchar   *id;
  GdkPixbuf     *pixbuf;
  gchar         *stock_id;

  target_entry.target = (gchar *) type;
  target_entry.flags = GTK_TARGET_SAME_APP;
  target_entry.info = 0;

  gtk_drag_source_set (item, GDK_BUTTON1_MASK,
                       &target_entry, 1,
                       GDK_ACTION_MOVE);

  if (is_separator)
    {
      id = "separator";

      pixbuf = _blxo_toolbars_new_separator_pixbuf ();
      if (G_LIKELY (pixbuf != NULL))
        {
          gtk_drag_source_set_icon_pixbuf (item, pixbuf);
          g_object_unref (G_OBJECT (pixbuf));
        }
    }
  else
    {
      id = gtk_action_get_name (action);

      g_object_get (G_OBJECT (action), "stock-id", &stock_id, NULL);
      if (G_UNLIKELY (stock_id == NULL))
        stock_id = g_strdup (GTK_STOCK_DND);

      pixbuf = gtk_widget_render_icon (item, stock_id, GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
      if (G_LIKELY (pixbuf != NULL))
        {
          gtk_drag_source_set_icon_pixbuf (item, pixbuf);
          g_object_unref (G_OBJECT (pixbuf));
        }

      g_free (stock_id);
    }

  g_object_set_data_full (G_OBJECT (item), I_("type"), g_strdup (type), g_free);
  g_object_set_data_full (G_OBJECT (item), I_("id"), g_strdup (id), g_free);
}



static GtkWidget*
blxo_toolbars_view_create_item_from_action (BlxoToolbarsView *view,
                                           const gchar     *action_name,
                                           const gchar     *type,
                                           gboolean         is_separator,
                                           GtkAction      **ret_action)
{
  GtkAction *action;
  GtkWidget *item;

  if (is_separator)
    {
      item = GTK_WIDGET (gtk_separator_tool_item_new ());
      action = NULL;
    }
  else
    {
      g_return_val_if_fail (action_name != NULL, NULL);

      g_signal_emit (G_OBJECT (view), toolbars_view_signals[ACTION_REQUEST],
                     0, action_name);

      action = _blxo_toolbars_find_action (view->priv->ui_manager, action_name);
      if (G_LIKELY (action != NULL))
        item = gtk_action_create_tool_item (action);
      else
        return NULL;
    }

  g_signal_connect (G_OBJECT (item), "drag-begin",
                    G_CALLBACK (gtk_widget_hide), view);
  g_signal_connect (G_OBJECT (item), "drag-end",
                    G_CALLBACK (gtk_widget_show), view);
  g_signal_connect (G_OBJECT (item), "drag-data-get",
                    G_CALLBACK (blxo_toolbars_view_drag_data_get), view);
  g_signal_connect (G_OBJECT (item), "drag-data-delete",
                    G_CALLBACK (blxo_toolbars_view_drag_data_delete), view);

  gtk_widget_show (item);

  if (view->priv->editing)
    {
      _blxo_toolbars_set_drag_cursor (item);
      gtk_widget_set_sensitive (item, TRUE);
      gtk_tool_item_set_use_drag_window (GTK_TOOL_ITEM (item), TRUE);
      set_item_drag_source (view->priv->model, item, action, is_separator, type);
    }

  if (ret_action != NULL)
    *ret_action = action;

  return item;
}



static GtkWidget*
blxo_toolbars_view_create_item (BlxoToolbarsView  *view,
                               BlxoToolbarsModel *model,
                               gint              toolbar_position,
                               gint              item_position,
                               GtkAction       **ret_action)
{
  const gchar *action_name;
  const gchar *type;
  gboolean     is_separator;

  blxo_toolbars_model_item_nth (model, toolbar_position, item_position,
                               &is_separator, &action_name, &type);

  return blxo_toolbars_view_create_item_from_action (view, action_name, type,
                                                    is_separator, ret_action);
}



static gboolean
data_is_separator (const char *data)
{
  return blxo_str_is_equal (data, "separator");
}



static void
blxo_toolbars_view_drag_data_received (GtkWidget         *toolbar,
                                      GdkDragContext    *context,
                                      gint               x,
                                      gint               y,
                                      GtkSelectionData  *selection_data,
                                      guint              info,
                                      guint              drag_time,
                                      BlxoToolbarsView   *view)
{
  GdkAtom target;
  gchar  *type;
  gchar  *id;
  gint    toolbar_position;
  gint    item_position;

  target = gtk_drag_dest_find_target (toolbar, context, NULL);
  type = blxo_toolbars_model_get_item_type (view->priv->model, target);
  id = blxo_toolbars_model_get_item_id (view->priv->model, type, (const gchar *) gtk_selection_data_get_data (selection_data));

  if (G_UNLIKELY (id == NULL))
    {
      view->priv->pending = FALSE;
      g_free (type);
      return;
    }

  if (view->priv->pending)
    {
      view->priv->pending = FALSE;
      view->priv->dragged_item = blxo_toolbars_view_create_item_from_action (view, id, type,
                                                                            data_is_separator (id),
                                                                            NULL);
      g_object_ref (G_OBJECT (view->priv->dragged_item));
      gtk_object_sink (GTK_OBJECT (view->priv->dragged_item));
    }
  else
    {
      item_position = gtk_toolbar_get_drop_index (GTK_TOOLBAR (toolbar), x, y);
      toolbar_position = blxo_toolbars_view_get_toolbar_position (view, toolbar);

      if (data_is_separator ((const gchar *) gtk_selection_data_get_data (selection_data)))
        blxo_toolbars_model_add_separator (view->priv->model, toolbar_position, item_position);
      else
        blxo_toolbars_model_add_item (view->priv->model, toolbar_position, item_position, id, type);

      gtk_drag_finish (context, TRUE, gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE, drag_time);
    }

  g_free (type);
  g_free (id);
}



static void
toolbar_style_activated (GtkWidget       *menuitem,
                         BlxoToolbarsView *view)
{
  GtkToolbarStyle style;
  gint            position;

  if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem)))
    {
      style = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem), I_("blxo-toolbar-style")));
      position = blxo_toolbars_view_get_toolbar_position (view, view->priv->selected_toolbar);

      if (style == 0)
        blxo_toolbars_model_unset_style (view->priv->model, position);
      else
        blxo_toolbars_model_set_style (view->priv->model, style - 1, position);
    }
}



static void
remove_toolbar_activated (GtkWidget       *menuitem,
                          BlxoToolbarsView *view)
{
  gint position;

  position = blxo_toolbars_view_get_toolbar_position (view, view->priv->selected_toolbar);
  blxo_toolbars_model_remove_toolbar (view->priv->model, position);
}



static void
customize_toolbar_activated (GtkWidget       *menuitem,
                             BlxoToolbarsView *view)
{
  g_signal_emit (G_OBJECT (view), toolbars_view_signals[CUSTOMIZE], 0);
}



static void
blxo_toolbars_view_context_menu (GtkWidget       *toolbar,
                                gint             x,
                                gint             y,
                                gint             button,
                                BlxoToolbarsView *view)
{
  BlxoToolbarsModelFlags flags;
  gint                  style = -1;
  GtkWidget            *submenu;
  GtkWidget            *menu;
  GtkWidget            *item;
  gint                  position;

  view->priv->selected_toolbar = toolbar;

  position = blxo_toolbars_view_get_toolbar_position (view, toolbar);
  flags = blxo_toolbars_model_get_flags (view->priv->model, position);
  if ((flags & BLXO_TOOLBARS_MODEL_OVERRIDE_STYLE) != 0)
    style = blxo_toolbars_model_get_style (view->priv->model, position);

  menu = gtk_menu_new ();

  item = gtk_image_menu_item_new_with_mnemonic (_("Toolbar _Style"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  submenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

  item = gtk_radio_menu_item_new_with_mnemonic (NULL, _("_Desktop Default"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), (style < 0));
  g_object_set_data (G_OBJECT (item), I_("blxo-toolbar-style"), GINT_TO_POINTER (0));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (toolbar_style_activated), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  gtk_widget_show (item);

  item = gtk_radio_menu_item_new_with_mnemonic_from_widget (GTK_RADIO_MENU_ITEM (item),
                                                            _("_Icons only"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), (style == GTK_TOOLBAR_ICONS));
  g_object_set_data (G_OBJECT (item), I_("blxo-toolbar-style"), GINT_TO_POINTER (GTK_TOOLBAR_ICONS + 1));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (toolbar_style_activated), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  gtk_widget_show (item);

  item = gtk_radio_menu_item_new_with_mnemonic_from_widget (GTK_RADIO_MENU_ITEM (item),
                                                            _("_Text only"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), (style == GTK_TOOLBAR_TEXT));
  g_object_set_data (G_OBJECT (item), I_("blxo-toolbar-style"), GINT_TO_POINTER (GTK_TOOLBAR_TEXT + 1));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (toolbar_style_activated), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  gtk_widget_show (item);

  item = gtk_radio_menu_item_new_with_mnemonic_from_widget (GTK_RADIO_MENU_ITEM (item),
                                                            _("Text for _All Icons"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), (style == GTK_TOOLBAR_BOTH));
  g_object_set_data (G_OBJECT (item), I_("blxo-toolbar-style"), GINT_TO_POINTER (GTK_TOOLBAR_BOTH + 1));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (toolbar_style_activated), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  gtk_widget_show (item);

  item = gtk_radio_menu_item_new_with_mnemonic_from_widget (GTK_RADIO_MENU_ITEM (item),
                                                            _("Text for I_mportant Icons"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), (style == GTK_TOOLBAR_BOTH_HORIZ));
  g_object_set_data (G_OBJECT (item), I_("blxo-toolbar-style"), GINT_TO_POINTER (GTK_TOOLBAR_BOTH_HORIZ + 1));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (toolbar_style_activated), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
  gtk_widget_show (item);

  item = gtk_image_menu_item_new_with_mnemonic (_("_Remove Toolbar"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (remove_toolbar_activated), view);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  if ((flags & BLXO_TOOLBARS_MODEL_NOT_REMOVABLE) != 0)
    gtk_widget_set_sensitive (item, FALSE);

  if (g_signal_has_handler_pending (G_OBJECT (view), toolbars_view_signals[CUSTOMIZE], 0, TRUE))
    {
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      item = gtk_image_menu_item_new_with_mnemonic (_("Customize Toolbar..."));
      g_signal_connect (G_OBJECT (item), "activate",
                        G_CALLBACK (customize_toolbar_activated), view);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      if (view->priv->editing)
        gtk_widget_set_sensitive (item, FALSE);
    }

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  button, gtk_get_current_event_time ());
}



static void
blxo_toolbars_view_free_dragged_item (BlxoToolbarsView *view)
{
  if (G_LIKELY (view->priv->dragged_item != NULL))
    {
      gtk_widget_destroy (GTK_WIDGET (view->priv->dragged_item));
      g_object_unref (G_OBJECT (view->priv->dragged_item));
      view->priv->dragged_item = NULL;
    }
}



static gboolean
blxo_toolbars_view_drag_drop (GtkWidget        *widget,
                             GdkDragContext   *context,
                             gint              x,
                             gint              y,
                             guint             drag_time,
                             BlxoToolbarsView  *view)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (G_LIKELY (target != GDK_NONE))
    {
      gtk_drag_get_data (widget, context, target, drag_time);
      return TRUE;
    }

  blxo_toolbars_view_free_dragged_item (view);

  return FALSE;
}



static gboolean
blxo_toolbars_view_drag_motion (GtkWidget        *toolbar,
                               GdkDragContext   *context,
                               gint              x,
                               gint              y,
                               guint             drag_time,
                               BlxoToolbarsView  *view)
{
  BlxoToolbarsModelFlags flags;
  GtkWidget            *source;
  gboolean              is_item;
  GdkAtom               target;
  gint                  toolbar_position;
  gint                  idx;
  GdkDragAction         suggested_action;

  source = gtk_drag_get_source_widget (context);
  suggested_action = gdk_drag_context_get_suggested_action (context);
  if (G_LIKELY (source != NULL))
    {
      toolbar_position = blxo_toolbars_view_get_toolbar_position (view, toolbar);
      flags = blxo_toolbars_model_get_flags (view->priv->model, toolbar_position);

      is_item = view->priv->editing
             && (gtk_widget_get_ancestor (source, BLXO_TYPE_TOOLBARS_VIEW)
              || gtk_widget_get_ancestor (source, BLXO_TYPE_TOOLBARS_EDITOR));

      if ((flags & BLXO_TOOLBARS_MODEL_ACCEPT_ITEMS_ONLY) != 0 && !is_item)
        {
          gdk_drag_status (context, 0, drag_time);
          return FALSE;
        }

      /* FIXME how to set the suggested action on the context? */
      if (gtk_widget_is_ancestor (source, toolbar))
        suggested_action = GDK_ACTION_MOVE;
    }

  target = gtk_drag_dest_find_target (toolbar, context, NULL);
  if (G_UNLIKELY (target == GDK_NONE))
    {
      gdk_drag_status (context, 0, drag_time);
      return FALSE;
    }

  if (view->priv->target_toolbar != toolbar)
    {
      if (view->priv->target_toolbar != NULL)
        {
          gtk_toolbar_set_drop_highlight_item (GTK_TOOLBAR (view->priv->target_toolbar),
                                               NULL, 0);
        }

      blxo_toolbars_view_free_dragged_item (view);
      view->priv->target_toolbar = toolbar;
      view->priv->pending = TRUE;

      gtk_drag_get_data (toolbar, context, target, drag_time);
    }

  if (view->priv->dragged_item != NULL && view->priv->editing)
    {
      idx = gtk_toolbar_get_drop_index (GTK_TOOLBAR (toolbar), x, y);
      gtk_toolbar_set_drop_highlight_item (GTK_TOOLBAR (toolbar),
                                           GTK_TOOL_ITEM (view->priv->dragged_item),
                                           idx);
    }

  gdk_drag_status (context, suggested_action, drag_time);

  return TRUE;
}



static void
blxo_toolbars_view_drag_leave (GtkWidget       *toolbar,
                              GdkDragContext  *context,
                              guint            drag_time,
                              BlxoToolbarsView *view)
{
  if (view->priv->target_toolbar == toolbar)
    {
      gtk_toolbar_set_drop_highlight_item (GTK_TOOLBAR (toolbar), NULL, 0);
      blxo_toolbars_view_free_dragged_item (view);
      view->priv->target_toolbar = NULL;
    }
}



static GtkWidget*
blxo_toolbars_view_create_dock (BlxoToolbarsView *view)
{
  GtkWidget *toolbar;
  GtkWidget *hbox;

  hbox = gtk_hbox_new (0, FALSE);
  gtk_widget_show (hbox);

  toolbar = g_object_new (GTK_TYPE_TOOLBAR,
                          "show-arrow", TRUE,
                          NULL);
  gtk_box_pack_start (GTK_BOX (hbox), toolbar, TRUE, TRUE, 0);
  gtk_widget_show (toolbar);

  gtk_drag_dest_set (toolbar, 0,
                     dst_targets, G_N_ELEMENTS (dst_targets),
                     GDK_ACTION_MOVE | GDK_ACTION_COPY);

  g_signal_connect (G_OBJECT (toolbar), "drag-drop",
                    G_CALLBACK (blxo_toolbars_view_drag_drop), view);
  g_signal_connect (G_OBJECT (toolbar), "drag-motion",
                    G_CALLBACK (blxo_toolbars_view_drag_motion), view);
  g_signal_connect (G_OBJECT (toolbar), "drag-leave",
                    G_CALLBACK (blxo_toolbars_view_drag_leave), view);
  g_signal_connect (G_OBJECT (toolbar), "drag-data-received",
                    G_CALLBACK (blxo_toolbars_view_drag_data_received), view);
  g_signal_connect (G_OBJECT (toolbar), "popup-context-menu",
                    G_CALLBACK (blxo_toolbars_view_context_menu), view);

  return hbox;
}



static void
blxo_toolbars_view_toolbar_added (BlxoToolbarsModel *model,
                                 gint              position,
                                 BlxoToolbarsView  *view)
{
  GtkWidget *dock;

  dock = blxo_toolbars_view_create_dock (view);
  gtk_widget_set_size_request (dock, -1, MIN_TOOLBAR_HEIGHT);
  gtk_box_pack_start (GTK_BOX (view), dock, TRUE, TRUE, 0);
  gtk_box_reorder_child (GTK_BOX (view), dock, position);
  gtk_widget_show_all (dock);
}



static void
blxo_toolbars_view_toolbar_changed (BlxoToolbarsModel *model,
                                   gint              position,
                                   BlxoToolbarsView  *view)
{
  BlxoToolbarsModelFlags flags;
  GtkToolbarStyle       style;
  GtkWidget            *toolbar;

  toolbar = blxo_toolbars_view_get_toolbar_nth (view, position);
  g_return_if_fail (toolbar != NULL);

  flags = blxo_toolbars_model_get_flags (model, position);
  if ((flags & BLXO_TOOLBARS_MODEL_OVERRIDE_STYLE) != 0)
    {
      style = blxo_toolbars_model_get_style (model, position);
      gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), style);
    }
  else
    {
      gtk_toolbar_unset_style (GTK_TOOLBAR (toolbar));
    }
}



static void
blxo_toolbars_view_toolbar_removed (BlxoToolbarsModel *model,
                                   gint              position,
                                   BlxoToolbarsView  *view)
{
  GtkWidget *toolbar;

  toolbar = blxo_toolbars_view_get_dock_nth (view, position);
  gtk_widget_destroy (toolbar);
}



static void
blxo_toolbars_view_item_added (BlxoToolbarsModel  *model,
                              gint               toolbar_position,
                              gint               item_position,
                              BlxoToolbarsView   *view)
{
  GtkWidget *toolbar;
  GtkWidget *dock;
  GtkWidget *item;
  GtkAction *action;

  g_return_if_fail (view->priv->ui_manager != NULL);

  toolbar = blxo_toolbars_view_get_toolbar_nth (view, toolbar_position);
  item = blxo_toolbars_view_create_item (view, model, toolbar_position,
                                        item_position, &action);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), item_position);

  dock = blxo_toolbars_view_get_dock_nth (view, toolbar_position);
  gtk_widget_set_size_request (dock, -1, -1);
  gtk_widget_queue_resize_no_redraw (dock);

  if (G_LIKELY (action != NULL))
    g_object_notify (G_OBJECT (action), "tooltip");
}



static void
blxo_toolbars_view_item_removed (BlxoToolbarsModel *model,
                                gint              toolbar_position,
                                gint              item_position,
                                BlxoToolbarsView  *view)
{
  GtkWidget *toolbar;
  GtkWidget *item;

  toolbar = blxo_toolbars_view_get_toolbar_nth (view, toolbar_position);
  item = GTK_WIDGET (gtk_toolbar_get_nth_item (GTK_TOOLBAR (toolbar), item_position));
  gtk_container_remove (GTK_CONTAINER (toolbar), item);

  if (blxo_toolbars_model_n_items (model, toolbar_position) == 0)
    blxo_toolbars_model_remove_toolbar (model, toolbar_position);
}



static void
blxo_toolbars_view_construct (BlxoToolbarsView *view)
{
  BlxoToolbarsModelFlags flags;
  GtkToolbarStyle       style;
  GtkAction            *action;
  GtkWidget            *toolbar;
  GtkWidget            *dock;
  GtkWidget            *item;
  gint                  n_toolbars;
  gint                  n_items;
  gint                  i;
  gint                  j;

  if (view->priv->model == NULL || view->priv->ui_manager == NULL)
    return;

  /* ensure there are no pending updates before getting the widgets */
  gtk_ui_manager_ensure_update (view->priv->ui_manager);

  n_toolbars = blxo_toolbars_model_n_toolbars (view->priv->model);
  for (i = 0; i < n_toolbars; ++i)
    {
      dock = blxo_toolbars_view_create_dock (view);
      gtk_box_pack_start (GTK_BOX (view), dock, TRUE, TRUE, 0);
      toolbar = blxo_toolbars_view_get_toolbar_nth (view, i);

      flags = blxo_toolbars_model_get_flags (view->priv->model, i);
      if ((flags & BLXO_TOOLBARS_MODEL_OVERRIDE_STYLE) != 0)
        {
          style = blxo_toolbars_model_get_style (view->priv->model, i);
          gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), style);
        }

      n_items = blxo_toolbars_model_n_items (view->priv->model, i);
      for (j = 0; j < n_items; ++j)
        {
          item = blxo_toolbars_view_create_item (view, view->priv->model,
                                                i, j, &action);
          if (G_LIKELY (item != NULL))
            {
              gtk_toolbar_insert (GTK_TOOLBAR (toolbar),
                                  GTK_TOOL_ITEM (item), j);

              if (G_LIKELY (action != NULL))
                g_object_notify (G_OBJECT (action), "tooltip");
            }
          else
            {
              /* this should not happen, but anyways... */
              blxo_toolbars_model_remove_item (view->priv->model, i, j);
              --j; --n_items;
            }
        }

      if (n_items == 0)
        gtk_widget_set_size_request (dock, -1, MIN_TOOLBAR_HEIGHT);
    }
}



static void
blxo_toolbars_view_deconstruct (BlxoToolbarsView *view)
{
  GList *children;
  GList *lp;

  children = gtk_container_get_children (GTK_CONTAINER (view));
  for (lp = children; lp != NULL; lp = lp->next)
    gtk_widget_destroy (GTK_WIDGET (lp->data));
  g_list_free (children);
}



/**
 * blxo_toolbars_view_new:
 * @ui_manager  : A #GtkUIManager.
 *
 * Creates a new #BlxoToolbarsView.
 *
 * Returns: A newly created #BlxoToolbarsView.
 **/
GtkWidget*
blxo_toolbars_view_new (GtkUIManager *ui_manager)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (ui_manager), NULL);

  return g_object_new (BLXO_TYPE_TOOLBARS_VIEW,
                       "ui-manager", ui_manager,
                       NULL);
}



/**
 * blxo_toolbars_view_new_with_model:
 * @ui_manager  : A #GtkUIManager.
 * @model       : An #BlxoToolbarsModel.
 *
 * Creates a new #BlxoToolbarsView and associates it with
 * @model.
 *
 * Returns: A newly created #BlxoToolbarsView.
 **/
GtkWidget*
blxo_toolbars_view_new_with_model (GtkUIManager      *ui_manager,
                                  BlxoToolbarsModel  *model)
{
  g_return_val_if_fail (GTK_IS_UI_MANAGER (ui_manager), NULL);
  g_return_val_if_fail (BLXO_IS_TOOLBARS_MODEL (model), NULL);

  return g_object_new (BLXO_TYPE_TOOLBARS_VIEW,
                       "ui-manager", ui_manager,
                       "model", model,
                       NULL);
}



/**
 * blxo_toolbars_view_get_editing:
 * @view  : An #BlxoToolbarsView.
 *
 * Gets wether @view is currently being edited.
 *
 * Returns: %TRUE if @view is currently being edited, else %FALSE.
 **/
gboolean
blxo_toolbars_view_get_editing (BlxoToolbarsView *view)
{
  g_return_val_if_fail (BLXO_IS_TOOLBARS_VIEW (view), FALSE);
  return view->priv->editing;
}



/**
 * blxo_toolbars_view_set_editing:
 * @view        : An #BlxoToolbarsView.
 * @editing     : New editing mode.
 *
 * Sets wether @view is currently being edited.
 **/
void
blxo_toolbars_view_set_editing (BlxoToolbarsView *view,
                               gboolean         editing)
{
  GtkToolItem *item;
  const gchar *type;
  const gchar *id;
  GtkAction   *action;
  GtkWidget   *toolbar;
  gboolean     is_separator;
  gint         n_toolbars;
  gint         n_items;
  gint         i;
  gint         j;

  g_return_if_fail (BLXO_IS_TOOLBARS_VIEW (view));

  view->priv->editing = editing;

  n_toolbars = blxo_toolbars_view_get_n_toolbars (view);
  for (i = 0; i < n_toolbars; ++i)
    {
      toolbar = blxo_toolbars_view_get_toolbar_nth (view, i);

      n_items = gtk_toolbar_get_n_items (GTK_TOOLBAR (toolbar));
      for (j = 0; j < n_items; ++j)
        {
          blxo_toolbars_model_item_nth (view->priv->model, i, j,
                                       &is_separator, &id, &type);
          action = _blxo_toolbars_find_action (view->priv->ui_manager, id);

          item = gtk_toolbar_get_nth_item (GTK_TOOLBAR (toolbar), j);
          gtk_tool_item_set_use_drag_window (item, editing);

          if (editing)
            {
              _blxo_toolbars_set_drag_cursor (GTK_WIDGET (item));
              gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
              set_item_drag_source (view->priv->model, GTK_WIDGET (item),
                                    action, is_separator, type);
            }
          else
            {
              _blxo_toolbars_unset_drag_cursor (GTK_WIDGET (item));
              gtk_drag_source_unset (GTK_WIDGET (item));

              if (!is_separator)
                g_object_notify (G_OBJECT (action), "sensitive");
            }
        }
    }
}



/**
 * blxo_toolbars_view_get_model:
 * @view        : An #BlxoToolbarsView.
 *
 * Returns the #BlxoToolbarsModel currently associated with
 * @view or %NULL if @view has no model.
 *
 * Returns: The #BlxoToolbarsModel associated with @view.
 **/
BlxoToolbarsModel*
blxo_toolbars_view_get_model (BlxoToolbarsView *view)
{
  g_return_val_if_fail (BLXO_IS_TOOLBARS_VIEW (view), NULL);
  return view->priv->model;
}



/**
 * blxo_toolbars_view_set_model:
 * @view  : An #BlxoToolbarsView.
 * @model : An #BlxoToolbarsModel or %NULL.
 *
 * Set the #BlxoToolbarsModel currently associated with
 * @view or %NULL to disconnect from the active model.
 **/
void
blxo_toolbars_view_set_model (BlxoToolbarsView  *view,
                             BlxoToolbarsModel *model)
{
  g_return_if_fail (BLXO_IS_TOOLBARS_VIEW (view));
  g_return_if_fail (BLXO_IS_TOOLBARS_MODEL (model) || model == NULL);

  if (G_UNLIKELY (model == view->priv->model))
    return;

  if (view->priv->model != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (view->priv->model),
                                            blxo_toolbars_view_item_added,
                                            view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (view->priv->model),
                                            blxo_toolbars_view_item_removed,
                                            view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (view->priv->model),
                                            blxo_toolbars_view_toolbar_added,
                                            view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (view->priv->model),
                                            blxo_toolbars_view_toolbar_changed,
                                            view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (view->priv->model),
                                            blxo_toolbars_view_toolbar_removed,
                                            view);

      blxo_toolbars_view_deconstruct (view);

      g_object_unref (G_OBJECT (view->priv->model));
    }

  view->priv->model = model;

  if (model != NULL)
    {
      g_object_ref (G_OBJECT (model));

      g_signal_connect (G_OBJECT (model), "item-added",
                        G_CALLBACK (blxo_toolbars_view_item_added), view);
      g_signal_connect (G_OBJECT (model), "item-removed",
                        G_CALLBACK (blxo_toolbars_view_item_removed), view);
      g_signal_connect (G_OBJECT (model), "toolbar-added",
                        G_CALLBACK (blxo_toolbars_view_toolbar_added), view);
      g_signal_connect (G_OBJECT (model), "toolbar-changed",
                        G_CALLBACK (blxo_toolbars_view_toolbar_changed), view);
      g_signal_connect (G_OBJECT (model), "toolbar-removed",
                        G_CALLBACK (blxo_toolbars_view_toolbar_removed), view);

      blxo_toolbars_view_construct (view);
    }

  g_object_notify (G_OBJECT (view), "model");
}



/**
 * blxo_toolbars_view_get_ui_manager:
 * @view        : An #BlxoToolbarsView.
 *
 * Returns the #GtkUIManager currently associated with @view or %NULL is
 * no ui-manager has been set.
 *
 * Returns: The #GtkUIManager associated with @view or %NULL.
 **/
GtkUIManager*
blxo_toolbars_view_get_ui_manager (BlxoToolbarsView *view)
{
  g_return_val_if_fail (BLXO_IS_TOOLBARS_VIEW (view), NULL);
  return view->priv->ui_manager;
}



/**
 * blxo_toolbars_view_set_ui_manager:
 * @view        : An #BlxoToolbarsView.
 * @ui_manager  : A #GtkUIManager or %NULL.
 *
 * Set the #GtkUIManager currently associated with @view or %NULL
 * to disconnect from the current ui-manager.
 **/
void
blxo_toolbars_view_set_ui_manager (BlxoToolbarsView *view,
                                  GtkUIManager    *ui_manager)
{
  g_return_if_fail (BLXO_IS_TOOLBARS_VIEW (view));
  g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager) || ui_manager == NULL);

  if (view->priv->ui_manager != NULL)
    {
      blxo_toolbars_view_deconstruct (view);

      g_object_unref (G_OBJECT (view->priv->ui_manager));
    }

  view->priv->ui_manager = ui_manager;

  if (ui_manager != NULL)
    {
      g_object_ref (G_OBJECT (ui_manager));

      blxo_toolbars_view_construct (view);
    }
}



#define __BLXO_TOOLBARS_VIEW_C__
#include <blxo/blxo-aliasdef.c>
