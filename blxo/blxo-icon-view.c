/*-
 * Copyright (c) 2008       Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2004-2006  os-cillation e.K.
 * Copyright (c) 2002,2004  Anders Carlsson <andersca@gnu.org>
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

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <blxo/blxo-config.h>
#include <blxo/blxo-enum-types.h>
#include <blxo/blxo-icon-view.h>
#include <blxo/blxo-cell-renderer-icon.h>
#include <blxo/blxo-marshal.h>
#include <blxo/blxo-private.h>
#include <blxo/blxo-string.h>
#include <blxo/blxo-alias.h>

/**
 * SECTION: blxo-icon-view
 * @title: BlxoIconView
 * @short_description: A widget which displays a list of icons in a grid
 * @include: blxo/blxo.h
 *
 * #BlxoIconView provides an alternative view on a list model.
 * It displays the model as a grid of icons with labels. Like
 * #GtkTreeView, it allows to select one or multiple items
 * (depending on the selection mode, see blxo_icon_view_set_selection_mode()).
 * In addition to selection with the arrow keys, #BlxoIconView supports
 * rubberband selection, which is controlled by dragging the pointer.
 **/

/* resurrect dead gdk apis for Gtk3
 * This is easier than using #ifs everywhere
 */
#if GTK_CHECK_VERSION (3, 0, 0)

# define GdkRectangle cairo_rectangle_int_t
# define GdkRegion    cairo_region_t
# define gdk_region_rectangle(rect) cairo_region_create_rectangle (rect)
# define gdk_region_destroy(region) cairo_region_destroy (region)
# define gdk_region_subtract(dst, other) cairo_region_subtract (dst, other)
# define gdk_region_union_with_rect(dst, rect) cairo_region_union_rectangle (dst, rect)

# ifdef gdk_cursor_unref
#   undef gdk_cursor_unref
# endif
# define gdk_cursor_unref(cursor) g_object_unref (cursor)

# endif

/* the search dialog timeout (in ms) */
#define BLXO_ICON_VIEW_SEARCH_DIALOG_TIMEOUT (5000)

#define SCROLL_EDGE_SIZE 15



/* Property identifiers */
enum
{
  PROP_0,
  PROP_PIXBUF_COLUMN,
  PROP_ICON_COLUMN,
  PROP_TEXT_COLUMN,
  PROP_MARKUP_COLUMN,
  PROP_SELECTION_MODE,
  PROP_LAYOUT_MODE,
  PROP_ORIENTATION,
  PROP_MODEL,
  PROP_COLUMNS,
  PROP_ITEM_WIDTH,
  PROP_SPACING,
  PROP_ROW_SPACING,
  PROP_COLUMN_SPACING,
  PROP_MARGIN,
  PROP_REORDERABLE,
  PROP_SINGLE_CLICK,
  PROP_SINGLE_CLICK_TIMEOUT,
  PROP_ENABLE_SEARCH,
  PROP_SEARCH_COLUMN,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_VSCROLL_POLICY,
  PROP_HSCROLL_POLICY
};

/* Signal identifiers */
enum
{
  ITEM_ACTIVATED,
  SELECTION_CHANGED,
  SELECT_ALL,
  UNSELECT_ALL,
  SELECT_CURSOR_ITEM,
  TOGGLE_CURSOR_ITEM,
  MOVE_CURSOR,
  ACTIVATE_CURSOR_ITEM,
  START_INTERACTIVE_SEARCH,
  LAST_SIGNAL
};

/* Icon view flags */
typedef enum
{
  BLXO_ICON_VIEW_DRAW_KEYFOCUS = (1l << 0),  /* whether to draw keyboard focus */
  BLXO_ICON_VIEW_ITERS_PERSIST = (1l << 1),  /* whether current model provides persistent iterators */
} BlxoIconViewFlags;

#define BLXO_ICON_VIEW_SET_FLAG(icon_view, flag)   G_STMT_START{ (BLXO_ICON_VIEW (icon_view)->priv->flags |= flag); }G_STMT_END
#define BLXO_ICON_VIEW_UNSET_FLAG(icon_view, flag) G_STMT_START{ (BLXO_ICON_VIEW (icon_view)->priv->flags &= ~(flag)); }G_STMT_END
#define BLXO_ICON_VIEW_FLAG_SET(icon_view, flag)   ((BLXO_ICON_VIEW (icon_view)->priv->flags & (flag)) == (flag))



typedef struct _BlxoIconViewCellInfo BlxoIconViewCellInfo;
typedef struct _BlxoIconViewChild    BlxoIconViewChild;
typedef struct _BlxoIconViewItem     BlxoIconViewItem;



#define BLXO_ICON_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BLXO_TYPE_ICON_VIEW, BlxoIconViewPrivate))
#define BLXO_ICON_VIEW_CELL_INFO(obj)   ((BlxoIconViewCellInfo *) (obj))
#define BLXO_ICON_VIEW_CHILD(obj)       ((BlxoIconViewChild *) (obj))
#define BLXO_ICON_VIEW_ITEM(obj)        ((BlxoIconViewItem *) (obj))



static void                 blxo_icon_view_cell_layout_init               (GtkCellLayoutIface     *iface);
static void                 blxo_icon_view_dispose                        (GObject                *object);
static void                 blxo_icon_view_finalize                       (GObject                *object);
static void                 blxo_icon_view_get_property                   (GObject                *object,
                                                                          guint                   prop_id,
                                                                          GValue                 *value,
                                                                          GParamSpec             *pspec);
static void                 blxo_icon_view_set_property                   (GObject                *object,
                                                                          guint                   prop_id,
                                                                          const GValue           *value,
                                                                          GParamSpec             *pspec);
static void                 blxo_icon_view_realize                        (GtkWidget              *widget);
static void                 blxo_icon_view_unrealize                      (GtkWidget              *widget);
#if !GTK_CHECK_VERSION (3, 0, 0)
static void                 blxo_icon_view_size_request                   (GtkWidget              *widget,
                                                                          GtkRequisition         *requisition);
#else
static void                 blxo_icon_view_get_preferred_width            (GtkWidget              *widget,
                                                                          gint                   *minimal_width,
                                                                          gint                   *natural_width);
static void                 blxo_icon_view_get_preferred_height           (GtkWidget              *widget,
                                                                          gint                   *minimal_height,
                                                                          gint                   *natural_height);
#endif
static void                 blxo_icon_view_size_allocate                  (GtkWidget              *widget,
                                                                          GtkAllocation          *allocation);
#if !GTK_CHECK_VERSION (3, 0, 0)
static void                 blxo_icon_view_style_set                      (GtkWidget              *widget,
                                                                          GtkStyle               *previous_style);
#endif
#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean             blxo_icon_view_draw                           (GtkWidget              *widget,
                                                                          cairo_t                *cr);
#else
static gboolean             blxo_icon_view_expose_event                   (GtkWidget              *widget,
                                                                          GdkEventExpose         *event);
#endif
static gboolean             blxo_icon_view_motion_notify_event            (GtkWidget              *widget,
                                                                          GdkEventMotion         *event);
static gboolean             blxo_icon_view_button_press_event             (GtkWidget              *widget,
                                                                          GdkEventButton         *event);
static gboolean             blxo_icon_view_button_release_event           (GtkWidget              *widget,
                                                                          GdkEventButton         *event);
static gboolean             blxo_icon_view_scroll_event                   (GtkWidget              *widget,
                                                                          GdkEventScroll         *event);
static gboolean             blxo_icon_view_key_press_event                (GtkWidget              *widget,
                                                                          GdkEventKey            *event);
static gboolean             blxo_icon_view_focus_out_event                (GtkWidget              *widget,
                                                                          GdkEventFocus          *event);
static gboolean             blxo_icon_view_leave_notify_event             (GtkWidget              *widget,
                                                                          GdkEventCrossing       *event);
static void                 blxo_icon_view_remove                         (GtkContainer           *container,
                                                                          GtkWidget              *widget);
static void                 blxo_icon_view_forall                         (GtkContainer           *container,
                                                                          gboolean                include_internals,
                                                                          GtkCallback             callback,
                                                                          gpointer                callback_data);
static void                 blxo_icon_view_set_adjustments                (BlxoIconView            *icon_view,
                                                                          GtkAdjustment          *hadj,
                                                                          GtkAdjustment          *vadj);
static void                 blxo_icon_view_real_select_all                (BlxoIconView            *icon_view);
static void                 blxo_icon_view_real_unselect_all              (BlxoIconView            *icon_view);
static void                 blxo_icon_view_real_select_cursor_item        (BlxoIconView            *icon_view);
static void                 blxo_icon_view_real_toggle_cursor_item        (BlxoIconView            *icon_view);
static gboolean             blxo_icon_view_real_activate_cursor_item      (BlxoIconView            *icon_view);
static gboolean             blxo_icon_view_real_start_interactive_search  (BlxoIconView            *icon_view);
static void                 blxo_icon_view_adjustment_changed             (GtkAdjustment          *adjustment,
                                                                          BlxoIconView            *icon_view);
static gint                 blxo_icon_view_layout_cols                    (BlxoIconView            *icon_view,
                                                                          gint                    item_height,
                                                                          gint                   *x,
                                                                          gint                   *maximum_height,
                                                                          gint                    max_rows);
static gint                 blxo_icon_view_layout_rows                    (BlxoIconView            *icon_view,
                                                                          gint                    item_width,
                                                                          gint                   *y,
                                                                          gint                   *maximum_width,
                                                                          gint                    max_cols);
static void                 blxo_icon_view_layout                         (BlxoIconView            *icon_view);
#if GTK_CHECK_VERSION (3, 0, 0)
static void                 blxo_icon_view_paint_item                     (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item,
                                                                          cairo_t                *cr,
                                                                          gint                    x,
                                                                          gint                    y,
                                                                          gboolean                draw_focus);
#else
static void                 blxo_icon_view_paint_item                     (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item,
                                                                          GdkRectangle           *area,
                                                                          GdkDrawable            *drawable,
                                                                          gint                    x,
                                                                          gint                    y,
                                                                          gboolean                draw_focus);

#endif
static void                 blxo_icon_view_queue_draw_item                (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item);
static void                 blxo_icon_view_queue_layout                   (BlxoIconView            *icon_view);
static void                 blxo_icon_view_set_cursor_item                (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item,
                                                                          gint                    cursor_cell);
static void                 blxo_icon_view_start_rubberbanding            (BlxoIconView            *icon_view,
                                                                          gint                    x,
                                                                          gint                    y);
static void                 blxo_icon_view_stop_rubberbanding             (BlxoIconView            *icon_view);
static void                 blxo_icon_view_update_rubberband_selection    (BlxoIconView            *icon_view);
static gboolean             blxo_icon_view_item_hit_test                  (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item,
                                                                          gint                    x,
                                                                          gint                    y,
                                                                          gint                    width,
                                                                          gint                    height);
static gboolean             blxo_icon_view_unselect_all_internal          (BlxoIconView            *icon_view);
static void                 blxo_icon_view_calculate_item_size            (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item);
static void                 blxo_icon_view_calculate_item_size2           (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item,
                                                                          gint                   *max_width,
                                                                          gint                   *max_height);
static void                 blxo_icon_view_update_rubberband              (gpointer                data);
static void                 blxo_icon_view_invalidate_sizes               (BlxoIconView            *icon_view);
static void                 blxo_icon_view_add_move_binding               (GtkBindingSet          *binding_set,
                                                                          guint                   keyval,
                                                                          guint                   modmask,
                                                                          GtkMovementStep         step,
                                                                          gint                    count);
static gboolean             blxo_icon_view_real_move_cursor               (BlxoIconView            *icon_view,
                                                                          GtkMovementStep         step,
                                                                          gint                    count);
static void                 blxo_icon_view_move_cursor_up_down            (BlxoIconView            *icon_view,
                                                                          gint                    count);
static void                 blxo_icon_view_move_cursor_page_up_down       (BlxoIconView            *icon_view,
                                                                          gint                    count);
static void                 blxo_icon_view_move_cursor_left_right         (BlxoIconView            *icon_view,
                                                                          gint                    count);
static void                 blxo_icon_view_move_cursor_start_end          (BlxoIconView            *icon_view,
                                                                          gint                    count);
static void                 blxo_icon_view_scroll_to_item                 (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item);
static void                 blxo_icon_view_select_item                    (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item);
static void                 blxo_icon_view_unselect_item                  (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item);
static gboolean             blxo_icon_view_select_all_between             (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *anchor,
                                                                          BlxoIconViewItem        *cursor);
static BlxoIconViewItem *    blxo_icon_view_get_item_at_coords             (const BlxoIconView      *icon_view,
                                                                          gint                    x,
                                                                          gint                    y,
                                                                          gboolean                only_in_cell,
                                                                          BlxoIconViewCellInfo   **cell_at_pos);
static void                 blxo_icon_view_get_cell_area                  (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item,
                                                                          BlxoIconViewCellInfo    *cell_info,
                                                                          GdkRectangle           *cell_area);
static BlxoIconViewCellInfo *blxo_icon_view_get_cell_info                  (BlxoIconView            *icon_view,
                                                                          GtkCellRenderer        *renderer);
static void                 blxo_icon_view_set_cell_data                  (const BlxoIconView      *icon_view,
                                                                          BlxoIconViewItem        *item);
static void                 blxo_icon_view_cell_layout_pack_start         (GtkCellLayout          *layout,
                                                                          GtkCellRenderer        *renderer,
                                                                          gboolean                expand);
static void                 blxo_icon_view_cell_layout_pack_end           (GtkCellLayout          *layout,
                                                                          GtkCellRenderer        *renderer,
                                                                          gboolean                expand);
static void                 blxo_icon_view_cell_layout_add_attribute      (GtkCellLayout          *layout,
                                                                          GtkCellRenderer        *renderer,
                                                                          const gchar            *attribute,
                                                                          gint                    column);
static void                 blxo_icon_view_cell_layout_clear              (GtkCellLayout          *layout);
static void                 blxo_icon_view_cell_layout_clear_attributes   (GtkCellLayout          *layout,
                                                                          GtkCellRenderer        *renderer);
static void                 blxo_icon_view_cell_layout_set_cell_data_func (GtkCellLayout          *layout,
                                                                          GtkCellRenderer        *cell,
                                                                          GtkCellLayoutDataFunc   func,
                                                                          gpointer                func_data,
                                                                          GDestroyNotify          destroy);
static void                 blxo_icon_view_cell_layout_reorder            (GtkCellLayout          *layout,
                                                                          GtkCellRenderer        *cell,
                                                                          gint                    position);
static void                 blxo_icon_view_item_activate_cell             (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item,
                                                                          BlxoIconViewCellInfo    *cell_info,
                                                                          GdkEvent               *event);
static void                 blxo_icon_view_put                            (BlxoIconView            *icon_view,
                                                                          GtkWidget              *widget,
                                                                          BlxoIconViewItem        *item,
                                                                          gint                    cell);
static void                 blxo_icon_view_remove_widget                  (GtkCellEditable        *editable,
                                                                          BlxoIconView            *icon_view);
static void                 blxo_icon_view_start_editing                  (BlxoIconView            *icon_view,
                                                                          BlxoIconViewItem        *item,
                                                                          BlxoIconViewCellInfo    *cell_info,
                                                                          GdkEvent               *event);
static void                 blxo_icon_view_stop_editing                   (BlxoIconView            *icon_view,
                                                                          gboolean                cancel_editing);
static void                 blxo_icon_view_set_pixbuf_column              (BlxoIconView            *icon_view,
                                                                          gint                    column);
static void                 blxo_icon_view_set_icon_column                (BlxoIconView            *icon_view,
                                                                          gint                    column);

static void                 blxo_icon_view_get_work_area_dimensions       (GdkWindow              *window,
                                                                          GdkRectangle           *dimensions);

/* Source side drag signals */
static void blxo_icon_view_drag_begin       (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void blxo_icon_view_drag_end         (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void blxo_icon_view_drag_data_get    (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            GtkSelectionData *selection_data,
                                            guint             info,
                                            guint             drag_time);
static void blxo_icon_view_drag_data_delete (GtkWidget        *widget,
                                            GdkDragContext   *context);

/* Target side drag signals */
static void     blxo_icon_view_drag_leave         (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  guint             drag_time);
static gboolean blxo_icon_view_drag_motion        (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             drag_time);
static gboolean blxo_icon_view_drag_drop          (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             drag_time);
static void     blxo_icon_view_drag_data_received (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  guint             drag_time);
static gboolean blxo_icon_view_maybe_begin_drag   (BlxoIconView      *icon_view,
                                                  GdkEventMotion   *event);

static void     remove_scroll_timeout            (BlxoIconView *icon_view);

/* single-click autoselection support */
static gboolean blxo_icon_view_single_click_timeout          (gpointer user_data);
static void     blxo_icon_view_single_click_timeout_destroy  (gpointer user_data);

/* Interactive search support */
static void     blxo_icon_view_search_activate           (GtkEntry       *entry,
                                                         BlxoIconView    *icon_view);
static void     blxo_icon_view_search_dialog_hide        (GtkWidget      *search_dialog,
                                                         BlxoIconView    *icon_view);
static void     blxo_icon_view_search_ensure_directory   (BlxoIconView    *icon_view);
static void     blxo_icon_view_search_init               (GtkWidget      *search_entry,
                                                         BlxoIconView    *icon_view);
static gboolean blxo_icon_view_search_iter               (BlxoIconView    *icon_view,
                                                         GtkTreeModel   *model,
                                                         GtkTreeIter    *iter,
                                                         const gchar    *text,
                                                         gint           *count,
                                                         gint            n);
static void     blxo_icon_view_search_move               (GtkWidget      *widget,
                                                         BlxoIconView    *icon_view,
                                                         gboolean        move_up);
static gboolean blxo_icon_view_search_start              (BlxoIconView    *icon_view,
                                                         gboolean        keybinding);
static gboolean blxo_icon_view_search_equal_func         (GtkTreeModel   *model,
                                                         gint            column,
                                                         const gchar    *key,
                                                         GtkTreeIter    *iter,
                                                         gpointer        user_data);
static void     blxo_icon_view_search_position_func      (BlxoIconView    *icon_view,
                                                         GtkWidget      *search_dialog,
                                                         gpointer        user_data);
static gboolean blxo_icon_view_search_button_press_event (GtkWidget      *widget,
                                                         GdkEventButton *event,
                                                         BlxoIconView    *icon_view);
static gboolean blxo_icon_view_search_delete_event       (GtkWidget      *widget,
                                                         GdkEventAny    *event,
                                                         BlxoIconView    *icon_view);
static gboolean blxo_icon_view_search_key_press_event    (GtkWidget      *widget,
                                                         GdkEventKey    *event,
                                                         BlxoIconView    *icon_view);
static gboolean blxo_icon_view_search_scroll_event       (GtkWidget      *widget,
                                                         GdkEventScroll *event,
                                                         BlxoIconView    *icon_view);
static gboolean blxo_icon_view_search_timeout            (gpointer        user_data);
static void     blxo_icon_view_search_timeout_destroy    (gpointer        user_data);



struct _BlxoIconViewCellInfo
{
  GtkCellRenderer      *cell;
  guint                 expand : 1;
  guint                 pack : 1;
  guint                 editing : 1;
  gint                  position;
  GSList               *attributes;
  GtkCellLayoutDataFunc func;
  gpointer              func_data;
  GDestroyNotify        destroy;
  gboolean              is_text;
};

struct _BlxoIconViewChild
{
  BlxoIconViewItem *item;
  GtkWidget       *widget;
  gint             cell;
};

struct _BlxoIconViewItem
{
  GtkTreeIter iter;

  /* Bounding box (a value of -1 for width indicates
   * that the item needs to be layouted first)
   */
  GdkRectangle area;

  /* Individual cells.
   * box[i] is the actual area occupied by cell i,
   * before, after are used to calculate the cell
   * area relative to the box.
   * See blxo_icon_view_get_cell_area().
   */
  gint n_cells;
  GdkRectangle *box;
  gint *before;
  gint *after;

  guint row : ((sizeof (guint) / 2) * 8) - 1;
  guint col : ((sizeof (guint) / 2) * 8) - 1;
  guint selected : 1;
  guint selected_before_rubberbanding : 1;
};

struct _BlxoIconViewPrivate
{
  gint width, height;
  gint rows, cols;

  GtkSelectionMode selection_mode;

  BlxoIconViewLayoutMode layout_mode;

  GdkWindow *bin_window;

  GList *children;

  GtkTreeModel *model;

  GList *items;

  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
#if GTK_CHECK_VERSION (3, 0, 0)
  GtkScrollablePolicy hscroll_policy;
  GtkScrollablePolicy vscroll_policy;
#endif

  guint layout_idle_id;

  gboolean doing_rubberband;
  gint rubberband_x_1, rubberband_y_1;
  gint rubberband_x2, rubberband_y2;

  guint scroll_timeout_id;
  gint scroll_value_diff;
  gint event_last_x, event_last_y;

  BlxoIconViewItem *anchor_item;
  BlxoIconViewItem *cursor_item;
  BlxoIconViewItem *edited_item;
  GtkCellEditable *editable;
  BlxoIconViewItem *prelit_item;

  BlxoIconViewItem *last_single_clicked;

  GList *cell_list;
  gint n_cells;

  gint cursor_cell;

  GtkOrientation orientation;

  gint columns;
  gint item_width;
  gint spacing;
  gint row_spacing;
  gint column_spacing;
  gint margin;

  gint text_column;
  gint markup_column;
  gint pixbuf_column;
  gint icon_column;

  gint pixbuf_cell;
  gint text_cell;

  /* Drag-and-drop. */
  GdkModifierType start_button_mask;
  gint pressed_button;
  gint press_start_x;
  gint press_start_y;

  GtkTargetList *source_targets;
  GdkDragAction source_actions;

  GtkTargetList *dest_targets;
  GdkDragAction dest_actions;

  GtkTreeRowReference *dest_item;
  BlxoIconViewDropPosition dest_pos;

  /* delayed scrolling */
  GtkTreeRowReference          *scroll_to_path;
  gfloat                        scroll_to_row_align;
  gfloat                        scroll_to_col_align;
  guint                         scroll_to_use_align : 1;

  /* misc flags */
  guint                         source_set : 1;
  guint                         dest_set : 1;
  guint                         reorderable : 1;
  guint                         empty_view_drop :1;

  guint                         ctrl_pressed : 1;
  guint                         shift_pressed : 1;

  /* Single-click support
   * The single_click_timeout is the timeout after which the
   * prelited item will be automatically selected in single
   * click mode (0 to disable).
   */
  guint                         single_click : 1;
  guint                         single_click_timeout;
  guint                         single_click_timeout_id;
  guint                         single_click_timeout_state;

  /* Interactive search support */
  guint                         enable_search : 1;
  gint                          search_column;
  gint                          search_selected_iter;
  guint                         search_timeout_id;
  gboolean                      search_disable_popdown;
  BlxoIconViewSearchEqualFunc    search_equal_func;
  gpointer                      search_equal_data;
  GDestroyNotify                search_equal_destroy;
  BlxoIconViewSearchPositionFunc search_position_func;
  gpointer                      search_position_data;
  GDestroyNotify                search_position_destroy;
  gint                          search_entry_changed_id;
  GtkWidget                    *search_entry;
  GtkWidget                    *search_window;

  /* BlxoIconViewFlags */
  guint flags;
};



#include <blxo/blxo-icon-view-accessible.c>



static guint icon_view_signals[LAST_SIGNAL];



G_DEFINE_TYPE_WITH_CODE (BlxoIconView, blxo_icon_view, GTK_TYPE_CONTAINER,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT, blxo_icon_view_cell_layout_init)
#if GTK_CHECK_VERSION (3, 0, 0)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL)
#endif
                        )

static AtkObject *
blxo_icon_view_get_accessible (GtkWidget *widget)
{
  static gboolean initited = FALSE;
  GType derived_type;
  AtkObjectFactory *factory;
  AtkRegistry *registry;
  GType derived_atk_type;

  if (!initited)
    {
      derived_type = g_type_parent (BLXO_TYPE_ICON_VIEW);

      registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (registry, derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);

      if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE))
        {
          atk_registry_set_factory_type (registry, BLXO_TYPE_ICON_VIEW,
                                         blxo_icon_view_accessible_factory_get_type ());
        }

      initited = TRUE;
    }

  return GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->get_accessible (widget);
}

static void
blxo_icon_view_get_work_area_dimensions (GdkWindow *window, GdkRectangle *dimensions)
{
  GdkDisplay   *display;
  GdkRectangle  geometry;

#if GTK_CHECK_VERSION(3, 22, 0)
  GdkMonitor   *monitor;

  display = gdk_window_get_display (window);
  monitor = gdk_display_get_monitor_at_window (display, window);
  gdk_monitor_get_workarea (monitor, &geometry);
#else
  GdkScreen    *screen;
  gint          num_monitor_at_window;

  display = gdk_window_get_display (window);
  screen = gdk_display_get_default_screen (display);
  num_monitor_at_window = gdk_screen_get_monitor_at_window (screen, window);
  gdk_screen_get_monitor_geometry (screen, num_monitor_at_window, &geometry);
#endif

  if (dimensions != NULL)
    {
       dimensions->x = geometry.x;
       dimensions->y = geometry.y;
       dimensions->width = geometry.width;
       dimensions->height = geometry.height;
    }
}



static void
blxo_icon_view_class_init (BlxoIconViewClass *klass)
{
  GtkContainerClass *gtkcontainer_class;
  GtkWidgetClass    *gtkwidget_class;
  GtkBindingSet     *gtkbinding_set;
  GObjectClass      *gobject_class;

  /* add our private data to the type's instances */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS /* GObject 2.58 */
  g_type_class_add_private (klass, sizeof (BlxoIconViewPrivate));
  G_GNUC_END_IGNORE_DEPRECATIONS

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = blxo_icon_view_dispose;
  gobject_class->finalize = blxo_icon_view_finalize;
  gobject_class->set_property = blxo_icon_view_set_property;
  gobject_class->get_property = blxo_icon_view_get_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = blxo_icon_view_realize;
  gtkwidget_class->unrealize = blxo_icon_view_unrealize;
#if GTK_CHECK_VERSION (3, 0, 0)
  gtkwidget_class->get_preferred_width = blxo_icon_view_get_preferred_width;
  gtkwidget_class->get_preferred_height = blxo_icon_view_get_preferred_height;
#else
  gtkwidget_class->size_request = blxo_icon_view_size_request;
#endif
  gtkwidget_class->size_allocate = blxo_icon_view_size_allocate;
#if !GTK_CHECK_VERSION (3, 0, 0)
  gtkwidget_class->style_set = blxo_icon_view_style_set;
#endif
  gtkwidget_class->get_accessible = blxo_icon_view_get_accessible;
#if GTK_CHECK_VERSION (3, 0, 0)
  gtkwidget_class->draw = blxo_icon_view_draw;
#else
  gtkwidget_class->expose_event = blxo_icon_view_expose_event;
#endif
  gtkwidget_class->motion_notify_event = blxo_icon_view_motion_notify_event;
  gtkwidget_class->button_press_event = blxo_icon_view_button_press_event;
  gtkwidget_class->button_release_event = blxo_icon_view_button_release_event;
  gtkwidget_class->scroll_event = blxo_icon_view_scroll_event;
  gtkwidget_class->key_press_event = blxo_icon_view_key_press_event;
  gtkwidget_class->focus_out_event = blxo_icon_view_focus_out_event;
  gtkwidget_class->leave_notify_event = blxo_icon_view_leave_notify_event;
  gtkwidget_class->drag_begin = blxo_icon_view_drag_begin;
  gtkwidget_class->drag_end = blxo_icon_view_drag_end;
  gtkwidget_class->drag_data_get = blxo_icon_view_drag_data_get;
  gtkwidget_class->drag_data_delete = blxo_icon_view_drag_data_delete;
  gtkwidget_class->drag_leave = blxo_icon_view_drag_leave;
  gtkwidget_class->drag_motion = blxo_icon_view_drag_motion;
  gtkwidget_class->drag_drop = blxo_icon_view_drag_drop;
  gtkwidget_class->drag_data_received = blxo_icon_view_drag_data_received;

  gtkcontainer_class = GTK_CONTAINER_CLASS (klass);
  gtkcontainer_class->remove = blxo_icon_view_remove;
  gtkcontainer_class->forall = blxo_icon_view_forall;

  klass->set_scroll_adjustments = blxo_icon_view_set_adjustments;
  klass->select_all = blxo_icon_view_real_select_all;
  klass->unselect_all = blxo_icon_view_real_unselect_all;
  klass->select_cursor_item = blxo_icon_view_real_select_cursor_item;
  klass->toggle_cursor_item = blxo_icon_view_real_toggle_cursor_item;
  klass->move_cursor = blxo_icon_view_real_move_cursor;
  klass->activate_cursor_item = blxo_icon_view_real_activate_cursor_item;
  klass->start_interactive_search = blxo_icon_view_real_start_interactive_search;

  /**
   * BlxoIconView:column-spacing:
   *
   * The column-spacing property specifies the space which is inserted between
   * the columns of the icon view.
   *
   * Since: 0.3.1
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COLUMN_SPACING,
                                   g_param_spec_int ("column-spacing",
                                                     _("Column Spacing"),
                                                     _("Space which is inserted between grid column"),
                                                     0, G_MAXINT, 6,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:columns:
   *
   * The columns property contains the number of the columns in which the
   * items should be displayed. If it is -1, the number of columns will
   * be chosen automatically to fill the available area.
   *
   * Since: 0.3.1
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COLUMNS,
                                   g_param_spec_int ("columns",
                                                     _("Number of columns"),
                                                     _("Number of columns to display"),
                                                     -1, G_MAXINT, -1,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:enable-search:
   *
   * View allows user to search through columns interactively.
   *
   * Since: 0.3.1.3
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ENABLE_SEARCH,
                                   g_param_spec_boolean ("enable-search",
                                                         _("Enable Search"),
                                                         _("View allows user to search through columns interactively"),
                                                         TRUE,
                                                         BLXO_PARAM_READWRITE));


  /**
   * BlxoIconView:item-width:
   *
   * The item-width property specifies the width to use for each item.
   * If it is set to -1, the icon view will automatically determine a
   * suitable item size.
   *
   * Since: 0.3.1
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ITEM_WIDTH,
                                   g_param_spec_int ("item-width",
                                                     _("Width for each item"),
                                                     _("The width used for each item"),
                                                     -1, G_MAXINT, -1,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:layout-mode:
   *
   * The layout-mode property specifies the way items are layed out in
   * the #BlxoIconView. This can be either %BLXO_ICON_VIEW_LAYOUT_ROWS,
   * which is the default, where items are layed out horizontally in
   * rows from top to bottom, or %BLXO_ICON_VIEW_LAYOUT_COLS, where items
   * are layed out vertically in columns from left to right.
   *
   * Since: 0.3.1.5
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT_MODE,
                                   g_param_spec_enum ("layout-mode",
                                                      _("Layout mode"),
                                                      _("The layout mode"),
                                                      BLXO_TYPE_ICON_VIEW_LAYOUT_MODE,
                                                      BLXO_ICON_VIEW_LAYOUT_ROWS,
                                                      BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:margin:
   *
   * The margin property specifies the space which is inserted
   * at the edges of the icon view.
   *
   * Since: 0.3.1
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN,
                                   g_param_spec_int ("margin",
                                                     _("Margin"),
                                                     _("Space which is inserted at the edges of the icon view"),
                                                     0, G_MAXINT, 6,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:markup-column:
   *
   * The markup-column property contains the number of the model column
   * containing markup information to be displayed. The markup column must be
   * of type #G_TYPE_STRING. If this property and the text-column property
   * are both set to column numbers, it overrides the text column.
   * If both are set to -1, no texts are displayed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MARKUP_COLUMN,
                                   g_param_spec_int ("markup-column",
                                                     _("Markup column"),
                                                     _("Model column used to retrieve the text if using Pango markup"),
                                                     -1, G_MAXINT, -1,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:model:
   *
   * The model property contains the #GtkTreeModel, which should be
   * display by this icon view. Setting this property to %NULL turns
   * off the display of anything.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        _("Icon View Model"),
                                                        _("The model for the icon view"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:orientation:
   *
   * The orientation property specifies how the cells (i.e. the icon and
   * the text) of the item are positioned relative to each other.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
                                                      _("Orientation"),
                                                      _("How the text and icon of each item are positioned relative to each other"),
                                                      GTK_TYPE_ORIENTATION,
                                                      GTK_ORIENTATION_VERTICAL,
                                                      BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:pixbuf-column:
   *
   * The ::pixbuf-column property contains the number of the model column
   * containing the pixbufs which are displayed. The pixbuf column must be
   * of type #GDK_TYPE_PIXBUF. Setting this property to -1 turns off the
   * display of pixbufs.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF_COLUMN,
                                   g_param_spec_int ("pixbuf-column",
                                                     _("Pixbuf column"),
                                                     _("Model column used to retrieve the icon pixbuf from"),
                                                     -1, G_MAXINT, -1,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:icon-column:
   *
   * The ::icon-column property contains the number of the model column
   * containing an absolute path to an image file to render. The icon column
   * must be of type #G_TYPE_STRING. Setting this property to -1 turns off
   * the display of icons.
   *
   * Since: 0.10.2
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_COLUMN,
                                   g_param_spec_int ("icon-column",
                                                     _("Icon column"),
                                                     _("Model column used to retrieve the absolute path of an image file to render"),
                                                     -1, G_MAXINT, -1,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:reorderable:
   *
   * The reorderable property specifies if the items can be reordered
   * by Drag and Drop.
   *
   * Since: 0.3.1
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_REORDERABLE,
                                   g_param_spec_boolean ("reorderable",
                                                         _("Reorderable"),
                                                         _("View is reorderable"),
                                                         FALSE,
                                                         BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:row-spacing:
   *
   * The row-spacing property specifies the space which is inserted between
   * the rows of the icon view.
   *
   * Since: 0.3.1
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ROW_SPACING,
                                   g_param_spec_int ("row-spacing",
                                                     _("Row Spacing"),
                                                     _("Space which is inserted between grid rows"),
                                                     0, G_MAXINT, 6,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:search-column:
   *
   * Model column to search through when searching through code.
   *
   * Since: 0.3.1.3
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SEARCH_COLUMN,
                                   g_param_spec_int ("search-column",
                                                     _("Search Column"),
                                                     _("Model column to search through when searching through item"),
                                                     -1, G_MAXINT, -1,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:selection-mode:
   *
   * The selection-mode property specifies the selection mode of
   * icon view. If the mode is #GTK_SELECTION_MULTIPLE, rubberband selection
   * is enabled, for the other modes, only keyboard selection is possible.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTION_MODE,
                                   g_param_spec_enum ("selection-mode",
                                                      _("Selection mode"),
                                                      _("The selection mode"),
                                                      GTK_TYPE_SELECTION_MODE,
                                                      GTK_SELECTION_SINGLE,
                                                      BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:single-click:
   *
   * Determines whether items can be activated by single or double clicks.
   *
   * Since: 0.3.1.3
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SINGLE_CLICK,
                                   g_param_spec_boolean ("single-click",
                                                         _("Single Click"),
                                                         _("Whether the items in the view can be activated with single clicks"),
                                                         FALSE,
                                                         BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:single-click-timeout:
   *
   * The amount of time in milliseconds after which a prelited item (an item
   * which is hovered by the mouse cursor) will be selected automatically in
   * single click mode. A value of %0 disables the automatic selection.
   *
   * Since: 0.3.1.5
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SINGLE_CLICK_TIMEOUT,
                                   g_param_spec_uint ("single-click-timeout",
                                                      _("Single Click Timeout"),
                                                      _("The amount of time after which the item under the mouse cursor will be selected automatically in single click mode"),
                                                      0, G_MAXUINT, 0,
                                                      BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:spacing:
   *
   * The spacing property specifies the space which is inserted between
   * the cells (i.e. the icon and the text) of an item.
   *
   * Since: 0.3.1
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
                                                     _("Spacing"),
                                                     _("Space which is inserted between cells of an item"),
                                                     0, G_MAXINT, 0,
                                                     BLXO_PARAM_READWRITE));

  /**
   * BlxoIconView:text-column:
   *
   * The text-column property contains the number of the model column
   * containing the texts which are displayed. The text column must be
   * of type #G_TYPE_STRING. If this property and the markup-column
   * property are both set to -1, no texts are displayed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT_COLUMN,
                                   g_param_spec_int ("text-column",
                                                     _("Text column"),
                                                     _("Model column used to retrieve the text from"),
                                                     -1, G_MAXINT, -1,
                                                     BLXO_PARAM_READWRITE));

#if GTK_CHECK_VERSION (3, 0, 0)
  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");
#endif

  /**
   * BlxoIconView::item-activated:
   * @icon_view : a #BlxoIconView.
   * @path      : the #GtkTreePath of the activated item.
   *
   * The ::item-activated signal is emitted when the method
   * blxo_icon_view_item_activated() is called, when the user double clicks
   * an item with the "activate-on-single-click" property set to %FALSE, or
   * when the user single clicks an item when the "activate-on-single-click"
   * property set to %TRUE. It is also emitted when a non-editable item is
   * selected and one of the keys: Space, Return or Enter is pressed.
   **/
  icon_view_signals[ITEM_ACTIVATED] =
    g_signal_new (I_("item-activated"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (BlxoIconViewClass, item_activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_TREE_PATH);

  /**
   * BlxoIconView::selection-changed:
   * @icon_view : a #BlxoIconView.
   *
   * The ::selection-changed signal is emitted when the selection
   * (i.e. the set of selected items) changes.
   **/
  icon_view_signals[SELECTION_CHANGED] =
    g_signal_new (I_("selection-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BlxoIconViewClass, selection_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

#if !GTK_CHECK_VERSION (3, 0, 0)
  /**
   * BlxoIconView::set-scroll-adjustments:
   * @icon_view   : a #BlxoIconView.
   * @hadjustment : the new horizontal #GtkAdjustment.
   * @vadjustment : the new vertical #GtkAdjustment.
   *
   * The ::set-scroll-adjustments signal is emitted when the scroll
   * adjustments have changed.
   **/
  gtkwidget_class->set_scroll_adjustments_signal =
    g_signal_new (I_("set-scroll-adjustments"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (BlxoIconViewClass, set_scroll_adjustments),
                  NULL, NULL,
                  _blxo_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
#endif

  /**
   * BlxoIconView::select-all:
   * @icon_view : a #BlxoIconView.
   *
   * A #GtkBindingSignal which gets emitted when the user selects all items.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   *
   * The default binding for this signal is Ctrl-a.
   **/
  icon_view_signals[SELECT_ALL] =
    g_signal_new (I_("select-all"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (BlxoIconViewClass, select_all),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * BlxoIconView::unselect-all:
   * @icon_view : a #BlxoIconView.
   *
   * A #GtkBindingSignal which gets emitted when the user unselects all items.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   *
   * The default binding for this signal is Ctrl-Shift-a.
   **/
  icon_view_signals[UNSELECT_ALL] =
    g_signal_new (I_("unselect-all"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (BlxoIconViewClass, unselect_all),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * BlxoIconView::select-cursor-item:
   * @icon_view : a #BlxoIconView.
   *
   * A #GtkBindingSignal which gets emitted when the user selects the item
   * that is currently focused.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   *
   * There is no default binding for this signal.
   **/
  icon_view_signals[SELECT_CURSOR_ITEM] =
    g_signal_new (I_("select-cursor-item"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (BlxoIconViewClass, select_cursor_item),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * BlxoIconView::toggle-cursor-item:
   * @icon_view : a #BlxoIconView.
   *
   * A #GtkBindingSignal which gets emitted when the user toggles whether
   * the currently focused item is selected or not. The exact effect of
   * this depend on the selection mode.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   *
   * There is no default binding for this signal is Ctrl-Space.
   **/
  icon_view_signals[TOGGLE_CURSOR_ITEM] =
    g_signal_new (I_("toggle-cursor-item"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (BlxoIconViewClass, toggle_cursor_item),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * BlxoIconView::activate-cursor-item:
   * @icon_view : a #BlxoIconView.
   *
   * A #GtkBindingSignal which gets emitted when the user activates the
   * currently focused item.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control activation
   * programmatically.
   *
   * The default bindings for this signal are Space, Return and Enter.
   **/
  icon_view_signals[ACTIVATE_CURSOR_ITEM] =
    g_signal_new (I_("activate-cursor-item"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (BlxoIconViewClass, activate_cursor_item),
                  NULL, NULL,
                  _blxo_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * BlxoIconView::start-interactive-search:
   * @icon_view : a #BlxoIconView.
   *
   * The ::start-interative-search signal is emitted when the user starts
   * typing to jump to an item in the icon view.
   **/
  icon_view_signals[START_INTERACTIVE_SEARCH] =
    g_signal_new (I_("start-interactive-search"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (BlxoIconViewClass, start_interactive_search),
                  NULL, NULL,
                  _blxo_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * BlxoIconView::move-cursor:
   * @icon_view : a #BlxoIconView.
   * @step      : the granularity of the move, as a #GtkMovementStep
   * @count     : the number of @step units to move
   *
   * The ::move-cursor signal is a keybinding signal which gets emitted when
   * the user initiates a cursor movement.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   * The default bindings for this signal include
   * * Arrow keys which move by individual steps
   * * Home/End keys which move to the first/last item
   * * PageUp/PageDown which move by "pages" All of these will extend the
   * selection when combined with the Shift modifier.
   **/
  icon_view_signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (BlxoIconViewClass, move_cursor),
                  NULL, NULL,
                  _blxo_marshal_BOOLEAN__ENUM_INT,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_MOVEMENT_STEP,
                  G_TYPE_INT);

  /* Key bindings */
  gtkbinding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_a, GDK_CONTROL_MASK, "select-all", 0);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_a, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "unselect-all", 0);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_space, GDK_CONTROL_MASK, "toggle-cursor-item", 0);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_space, 0, "activate-cursor-item", 0);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_Return, 0, "activate-cursor-item", 0);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_ISO_Enter, 0, "activate-cursor-item", 0);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_KP_Enter, 0, "activate-cursor-item", 0);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_f, GDK_CONTROL_MASK, "start-interactive-search", 0);
  gtk_binding_entry_add_signal (gtkbinding_set, GDK_KEY_F, GDK_CONTROL_MASK, "start-interactive-search", 0);

  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_Up, 0, GTK_MOVEMENT_DISPLAY_LINES, -1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_KP_Up, 0, GTK_MOVEMENT_DISPLAY_LINES, -1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_Down, 0, GTK_MOVEMENT_DISPLAY_LINES, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_KP_Down, 0, GTK_MOVEMENT_DISPLAY_LINES, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_p, GDK_CONTROL_MASK, GTK_MOVEMENT_DISPLAY_LINES, -1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_n, GDK_CONTROL_MASK, GTK_MOVEMENT_DISPLAY_LINES, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_Home, 0, GTK_MOVEMENT_BUFFER_ENDS, -1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_KP_Home, 0, GTK_MOVEMENT_BUFFER_ENDS, -1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_End, 0, GTK_MOVEMENT_BUFFER_ENDS, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_KP_End, 0, GTK_MOVEMENT_BUFFER_ENDS, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_Page_Up, 0, GTK_MOVEMENT_PAGES, -1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_KP_Page_Up, 0, GTK_MOVEMENT_PAGES, -1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_Page_Down, 0, GTK_MOVEMENT_PAGES, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_KP_Page_Down, 0, GTK_MOVEMENT_PAGES, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_Right, 0, GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_Left, 0, GTK_MOVEMENT_VISUAL_POSITIONS, -1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_KP_Right, 0, GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  blxo_icon_view_add_move_binding (gtkbinding_set, GDK_KEY_KP_Left, 0, GTK_MOVEMENT_VISUAL_POSITIONS, -1);
}



static void
blxo_icon_view_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start = blxo_icon_view_cell_layout_pack_start;
  iface->pack_end = blxo_icon_view_cell_layout_pack_end;
  iface->clear = blxo_icon_view_cell_layout_clear;
  iface->add_attribute = blxo_icon_view_cell_layout_add_attribute;
  iface->set_cell_data_func = blxo_icon_view_cell_layout_set_cell_data_func;
  iface->clear_attributes = blxo_icon_view_cell_layout_clear_attributes;
  iface->reorder = blxo_icon_view_cell_layout_reorder;
}



static void
blxo_icon_view_init (BlxoIconView *icon_view)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (icon_view)),
                               GTK_STYLE_CLASS_VIEW);
#endif

  icon_view->priv = BLXO_ICON_VIEW_GET_PRIVATE (icon_view);

  icon_view->priv->selection_mode = GTK_SELECTION_SINGLE;
  icon_view->priv->pressed_button = -1;
  icon_view->priv->press_start_x = -1;
  icon_view->priv->press_start_y = -1;
  icon_view->priv->text_column = -1;
  icon_view->priv->markup_column = -1;
  icon_view->priv->pixbuf_column = -1;
  icon_view->priv->icon_column = -1;
  icon_view->priv->text_cell = -1;
  icon_view->priv->pixbuf_cell = -1;

  gtk_widget_set_can_focus (GTK_WIDGET (icon_view), TRUE);

  blxo_icon_view_set_adjustments (icon_view, NULL, NULL);

  icon_view->priv->cursor_cell = -1;

  icon_view->priv->orientation = GTK_ORIENTATION_VERTICAL;

  icon_view->priv->columns = -1;
  icon_view->priv->item_width = -1;
  icon_view->priv->row_spacing = 6;
  icon_view->priv->column_spacing = 6;
  icon_view->priv->margin = 6;

  icon_view->priv->enable_search = TRUE;
  icon_view->priv->search_column = -1;
  icon_view->priv->search_equal_func = blxo_icon_view_search_equal_func;
  icon_view->priv->search_position_func = blxo_icon_view_search_position_func;

  icon_view->priv->flags = BLXO_ICON_VIEW_DRAW_KEYFOCUS;
}



static void
blxo_icon_view_dispose (GObject *object)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (object);

  /* cancel any pending search timeout */
  if (G_UNLIKELY (icon_view->priv->search_timeout_id != 0))
    g_source_remove (icon_view->priv->search_timeout_id);

  /* destroy the interactive search dialog */
  if (G_UNLIKELY (icon_view->priv->search_window != NULL))
    {
      gtk_widget_destroy (icon_view->priv->search_window);
      icon_view->priv->search_entry = NULL;
      icon_view->priv->search_window = NULL;
    }

  /* drop search equal and position functions (if any) */
  blxo_icon_view_set_search_equal_func (icon_view, NULL, NULL, NULL);
  blxo_icon_view_set_search_position_func (icon_view, NULL, NULL, NULL);

  /* reset the drag dest item */
  blxo_icon_view_set_drag_dest_item (icon_view, NULL, BLXO_ICON_VIEW_NO_DROP);

  /* drop the scroll to path (if any) */
  if (G_UNLIKELY (icon_view->priv->scroll_to_path != NULL))
    {
      gtk_tree_row_reference_free (icon_view->priv->scroll_to_path);
      icon_view->priv->scroll_to_path = NULL;
    }

  /* reset the model (also stops any active editing) */
  blxo_icon_view_set_model (icon_view, NULL);

  /* drop the scroll timer */
  remove_scroll_timeout (icon_view);

  (*G_OBJECT_CLASS (blxo_icon_view_parent_class)->dispose) (object);
}



static void
blxo_icon_view_finalize (GObject *object)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (object);

  /* drop the scroll adjustments */
  g_object_unref (G_OBJECT (icon_view->priv->hadjustment));
  g_object_unref (G_OBJECT (icon_view->priv->vadjustment));

  /* drop the cell renderers */
  blxo_icon_view_cell_layout_clear (GTK_CELL_LAYOUT (icon_view));

  /* be sure to cancel the single click timeout */
  if (G_UNLIKELY (icon_view->priv->single_click_timeout_id != 0))
    g_source_remove (icon_view->priv->single_click_timeout_id);

  /* kill the layout idle source (it's important to have this last!) */
  if (G_UNLIKELY (icon_view->priv->layout_idle_id != 0))
    g_source_remove (icon_view->priv->layout_idle_id);

  (*G_OBJECT_CLASS (blxo_icon_view_parent_class)->finalize) (object);
}


static void
blxo_icon_view_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
  const BlxoIconViewPrivate *priv = BLXO_ICON_VIEW (object)->priv;

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      g_value_set_int (value, priv->column_spacing);
      break;

    case PROP_COLUMNS:
      g_value_set_int (value, priv->columns);
      break;

    case PROP_ENABLE_SEARCH:
      g_value_set_boolean (value, priv->enable_search);
      break;

    case PROP_ITEM_WIDTH:
      g_value_set_int (value, priv->item_width);
      break;

    case PROP_MARGIN:
      g_value_set_int (value, priv->margin);
      break;

    case PROP_MARKUP_COLUMN:
      g_value_set_int (value, priv->markup_column);
      break;

    case PROP_MODEL:
      g_value_set_object (value, priv->model);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_PIXBUF_COLUMN:
      g_value_set_int (value, priv->pixbuf_column);
      break;

    case PROP_ICON_COLUMN:
      g_value_set_int (value, priv->icon_column);
      break;

    case PROP_REORDERABLE:
      g_value_set_boolean (value, priv->reorderable);
      break;

    case PROP_ROW_SPACING:
      g_value_set_int (value, priv->row_spacing);
      break;

    case PROP_SEARCH_COLUMN:
      g_value_set_int (value, priv->search_column);
      break;

    case PROP_SELECTION_MODE:
      g_value_set_enum (value, priv->selection_mode);
      break;

    case PROP_SINGLE_CLICK:
      g_value_set_boolean (value, priv->single_click);
      break;

    case PROP_SINGLE_CLICK_TIMEOUT:
      g_value_set_uint (value, priv->single_click_timeout);
      break;

    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;

    case PROP_TEXT_COLUMN:
      g_value_set_int (value, priv->text_column);
      break;

    case PROP_LAYOUT_MODE:
      g_value_set_enum (value, priv->layout_mode);
      break;

#if GTK_CHECK_VERSION (3, 0, 0)
    case PROP_HADJUSTMENT:
      g_value_set_object (value, priv->hadjustment);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, priv->vadjustment);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, priv->hscroll_policy);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, priv->vscroll_policy);
      break;
#endif

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
blxo_icon_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (object);

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      blxo_icon_view_set_column_spacing (icon_view, g_value_get_int (value));
      break;

    case PROP_COLUMNS:
      blxo_icon_view_set_columns (icon_view, g_value_get_int (value));
      break;

    case PROP_ENABLE_SEARCH:
      blxo_icon_view_set_enable_search (icon_view, g_value_get_boolean (value));
      break;

    case PROP_ITEM_WIDTH:
      blxo_icon_view_set_item_width (icon_view, g_value_get_int (value));
      break;

    case PROP_MARGIN:
      blxo_icon_view_set_margin (icon_view, g_value_get_int (value));
      break;

    case PROP_MODEL:
      blxo_icon_view_set_model (icon_view, g_value_get_object (value));
      break;

    case PROP_ORIENTATION:
      blxo_icon_view_set_orientation (icon_view, g_value_get_enum (value));
      break;

    case PROP_PIXBUF_COLUMN:
      blxo_icon_view_set_pixbuf_column (icon_view, g_value_get_int (value));
      break;

    case PROP_ICON_COLUMN:
      blxo_icon_view_set_icon_column (icon_view, g_value_get_int (value));
      break;

    case PROP_REORDERABLE:
      blxo_icon_view_set_reorderable (icon_view, g_value_get_boolean (value));
      break;

    case PROP_ROW_SPACING:
      blxo_icon_view_set_row_spacing (icon_view, g_value_get_int (value));
      break;

    case PROP_SEARCH_COLUMN:
      blxo_icon_view_set_search_column (icon_view, g_value_get_int (value));
      break;

    case PROP_SELECTION_MODE:
      blxo_icon_view_set_selection_mode (icon_view, g_value_get_enum (value));
      break;

    case PROP_SINGLE_CLICK:
      blxo_icon_view_set_single_click (icon_view, g_value_get_boolean (value));
      break;

    case PROP_SINGLE_CLICK_TIMEOUT:
      blxo_icon_view_set_single_click_timeout (icon_view, g_value_get_uint (value));
      break;

    case PROP_SPACING:
      blxo_icon_view_set_spacing (icon_view, g_value_get_int (value));
      break;

    case PROP_LAYOUT_MODE:
      blxo_icon_view_set_layout_mode (icon_view, g_value_get_enum (value));
      break;

#if GTK_CHECK_VERSION (3, 0, 0)
    case PROP_HADJUSTMENT:
      blxo_icon_view_set_adjustments (icon_view, g_value_get_object (value), icon_view->priv->vadjustment);
      break;

    case PROP_VADJUSTMENT:
      blxo_icon_view_set_adjustments (icon_view, icon_view->priv->hadjustment, g_value_get_object (value));
      break;

    case PROP_HSCROLL_POLICY:
      if (icon_view->priv->hscroll_policy != (GtkScrollablePolicy)g_value_get_enum (value))
        {
          icon_view->priv->hscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (icon_view));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_VSCROLL_POLICY:
      if (icon_view->priv->vscroll_policy != (GtkScrollablePolicy)g_value_get_enum (value))
        {
          icon_view->priv->vscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (icon_view));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
#endif

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
blxo_icon_view_realize (GtkWidget *widget)
{
  BlxoIconViewPrivate *priv = BLXO_ICON_VIEW (widget)->priv;
  GdkWindowAttr       attributes;
  gint                attributes_mask;
  GtkAllocation       allocation;

  gtk_widget_set_realized (widget, TRUE);
  gtk_widget_get_allocation (widget, &allocation);

  /* Allocate the clipping window */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
#if !GTK_CHECK_VERSION (3, 0, 0)
  attributes.colormap = gtk_widget_get_colormap (widget);
#else
#  define GDK_WA_COLORMAP 0
#endif
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  gtk_widget_set_window (widget, gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask));
  gdk_window_set_user_data (gtk_widget_get_window (widget), widget);

  /* Allocate the icons window */
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = MAX (priv->width, allocation.width);
  attributes.height = MAX (priv->height, allocation.height);
  attributes.event_mask = GDK_EXPOSURE_MASK
                        | GDK_SCROLL_MASK
                        | GDK_POINTER_MOTION_MASK
                        | GDK_LEAVE_NOTIFY_MASK
                        | GDK_BUTTON_PRESS_MASK
                        | GDK_BUTTON_RELEASE_MASK
                        | GDK_KEY_PRESS_MASK
                        | GDK_KEY_RELEASE_MASK
                        | gtk_widget_get_events (widget);
  priv->bin_window = gdk_window_new (gtk_widget_get_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (priv->bin_window, widget);

  /* Gtk2: Attach style/background - Gtk3 is rendered using CSS */
#if !GTK_CHECK_VERSION (3, 0, 0)
  gtk_widget_set_style (widget, gtk_style_attach (gtk_widget_get_style (widget), gtk_widget_get_window (widget)));
  gdk_window_set_background (priv->bin_window, &gtk_widget_get_style (widget)->base[gtk_widget_get_state (widget)]);
  gdk_window_set_background (gtk_widget_get_window (widget), &gtk_widget_get_style (widget)->base[gtk_widget_get_state (widget)]);
#endif

  /* map the icons window */
  gdk_window_show (priv->bin_window);
}

static void
blxo_icon_view_unrealize (GtkWidget *widget)
{
  BlxoIconViewPrivate *priv = BLXO_ICON_VIEW (widget)->priv;

  /* drop the icons window */
  gdk_window_set_user_data (priv->bin_window, NULL);
  gdk_window_destroy (priv->bin_window);
  priv->bin_window = NULL;

  /* let GtkWidget destroy children and widget->window */
  if (GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->unrealize)
    (*GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->unrealize) (widget);
}

#if GTK_CHECK_VERSION (3, 0, 0)
static void
blxo_icon_view_get_preferred_width (GtkWidget *widget,
                                   gint      *minimal_width,
                                   gint      *natural_width)
{
  const BlxoIconViewPrivate *priv = BLXO_ICON_VIEW (widget)->priv;
  BlxoIconViewChild         *child;
  gint                      child_minimal, child_natural;
  GList                    *lp;

  /* well, this is easy */
  *minimal_width = *natural_width = priv->width;

  /* handle the child widgets */
  for (lp = priv->children; lp != NULL; lp = lp->next)
    {
      child = lp->data;
      if (gtk_widget_get_visible (child->widget))
        gtk_widget_get_preferred_width (child->widget, &child_minimal, &child_natural);
    }
}

static void
blxo_icon_view_get_preferred_height (GtkWidget *widget,
                                    gint      *minimal_height,
                                    gint      *natural_height)
{
  const BlxoIconViewPrivate *priv = BLXO_ICON_VIEW (widget)->priv;
  BlxoIconViewChild         *child;
  gint                      child_minimal, child_natural;
  GList                    *lp;

  /* well, this is easy */
  *natural_height = *minimal_height = priv->height;

  /* handle the child widgets */
  for (lp = priv->children; lp != NULL; lp = lp->next)
    {
      child = lp->data;
      if (gtk_widget_get_visible (child->widget))
        gtk_widget_get_preferred_height (child->widget, &child_minimal, &child_natural);
    }
}

#else

static void
blxo_icon_view_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  const BlxoIconViewPrivate *priv = BLXO_ICON_VIEW (widget)->priv;
  BlxoIconViewChild         *child;
  GtkRequisition            child_requisition;
  GList                    *lp;

  /* well, this is easy */
  requisition->width = priv->width;
  requisition->height = priv->height;

  /* handle the child widgets */
  for (lp = priv->children; lp != NULL; lp = lp->next)
    {
      child = lp->data;
      if (gtk_widget_get_visible (child->widget))
        gtk_widget_size_request (child->widget, &child_requisition);
    }
}

#endif

static void
blxo_icon_view_allocate_children (BlxoIconView *icon_view)
{
  const BlxoIconViewPrivate *priv = icon_view->priv;
  const BlxoIconViewChild   *child;
  GtkAllocation             allocation;
  const GList              *lp;
  gint                      focus_line_width;
  gint                      focus_padding;

  for (lp = priv->children; lp != NULL; lp = lp->next)
    {
      child = BLXO_ICON_VIEW_CHILD (lp->data);

      /* totally ignore our child's requisition */
      if (child->cell < 0)
        allocation = child->item->area;
      else
        allocation = child->item->box[child->cell];

      /* increase the item area by focus width/padding */
      gtk_widget_style_get (GTK_WIDGET (icon_view), "focus-line-width", &focus_line_width, "focus-padding", &focus_padding, NULL);
      allocation.x = MAX (0, allocation.x - (focus_line_width + focus_padding));
      allocation.y = MAX (0, allocation.y - (focus_line_width + focus_padding));
      allocation.width = MIN (priv->width - allocation.x, allocation.width + 2 * (focus_line_width + focus_padding));
      allocation.height = MIN (priv->height - allocation.y, allocation.height + 2 * (focus_line_width + focus_padding));

      /* allocate the area to the child */
      gtk_widget_size_allocate (child->widget, &allocation);
    }
}



static void
blxo_icon_view_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  BlxoIconView   *icon_view = BLXO_ICON_VIEW (widget);

  /* apply the new size allocation */
  gtk_widget_set_allocation (widget, allocation);

  /* move/resize the clipping window, the icons window
   * will be handled by blxo_icon_view_layout().
   */
  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget), allocation->x, allocation->y, allocation->width, allocation->height);

  /* layout the items */
  blxo_icon_view_layout (icon_view);

  /* allocate space to the widgets (editing) */
  blxo_icon_view_allocate_children (icon_view);

  /* update the horizontal scroll adjustment accordingly */
  hadjustment = icon_view->priv->hadjustment;
  gtk_adjustment_set_page_size (hadjustment, allocation->width);
  gtk_adjustment_set_page_increment (hadjustment, allocation->width * 0.9);
  gtk_adjustment_set_step_increment (hadjustment, allocation->width * 0.1);
  gtk_adjustment_set_lower (hadjustment, 0);
  gtk_adjustment_set_upper (hadjustment, MAX (allocation->width, icon_view->priv->width));
  if (gtk_adjustment_get_value (hadjustment) > gtk_adjustment_get_upper (hadjustment) - gtk_adjustment_get_lower (hadjustment))
    gtk_adjustment_set_value (hadjustment, MAX (0, gtk_adjustment_get_upper (hadjustment) - gtk_adjustment_get_page_size (hadjustment)));

  /* update the vertical scroll adjustment accordingly */
  vadjustment = icon_view->priv->vadjustment;
  gtk_adjustment_set_page_size (vadjustment, allocation->height);
  gtk_adjustment_set_page_increment (vadjustment, allocation->height * 0.9);
  gtk_adjustment_set_step_increment (vadjustment, allocation->height * 0.1);
  gtk_adjustment_set_lower (vadjustment, 0);
  gtk_adjustment_set_upper (vadjustment, MAX (allocation->height, icon_view->priv->height));
  if (gtk_adjustment_get_value (vadjustment) > gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment))
    gtk_adjustment_set_value (vadjustment, MAX (0, gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment)));

  /* Prior to GTK 3.18, we need to emit "changed" ourselves */
#if !GTK_CHECK_VERSION (3, 18, 0)
  gtk_adjustment_changed (hadjustment);
  gtk_adjustment_changed (vadjustment);
#endif
}



#if !GTK_CHECK_VERSION (3, 0, 0)
static void
blxo_icon_view_style_set (GtkWidget *widget,
                         GtkStyle  *previous_style)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (widget);

  /* let GtkWidget do its work */
  (*GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->style_set) (widget, previous_style);

  /* apply the new style for the bin_window if we're realized */
  if (gtk_widget_get_realized (widget))
    gdk_window_set_background (icon_view->priv->bin_window, &gtk_widget_get_style (widget)->base[gtk_widget_get_state (widget)]);
}
#endif



#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean
blxo_icon_view_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  BlxoIconViewDropPosition dest_pos;
  BlxoIconViewPrivate     *priv = BLXO_ICON_VIEW (widget)->priv;
  BlxoIconViewItem        *dest_item = NULL;
  BlxoIconViewItem        *item;
  BlxoIconView            *icon_view = BLXO_ICON_VIEW (widget);
  GtkTreePath            *path;
  GdkRectangle            rect;
  GdkRectangle            clip;
  GdkRectangle            paint_area;
  const GList            *lp;
  gint                    dest_index = -1;
  GtkStyleContext        *context;

  /* verify that the expose happened on the icon window */
  if (!gtk_cairo_should_draw_window (cr, priv->bin_window))
    return FALSE;

  /* don't handle expose if the layout isn't done yet; the layout
   * method will schedule a redraw when done.
   */
  if (G_UNLIKELY (priv->layout_idle_id != 0))
    return FALSE;

  /* "returns [...] FALSE if all of cr is clipped and all drawing can be skipped" [sic] */
  if (!gdk_cairo_get_clip_rectangle (cr, &clip))
    return FALSE;

  context = gtk_widget_get_style_context (widget);

  /* draw a background according to the css theme */
  gtk_render_background (context, cr,
                         0, 0,
                         gtk_widget_get_allocated_width (widget),
                         gtk_widget_get_allocated_height (widget));

  /* transform coordinates so our old calculations work */
  gtk_cairo_transform_to_window (cr, widget, icon_view->priv->bin_window);

  /* retrieve the clipping rectangle again, with the transformed coordinates */
  gdk_cairo_get_clip_rectangle (cr, &clip);

  /* scroll to the previously remembered path (if any) */
  if (G_UNLIKELY (priv->scroll_to_path != NULL))
    {
      /* grab the path from the reference and invalidate the reference */
      path = gtk_tree_row_reference_get_path (priv->scroll_to_path);
      gtk_tree_row_reference_free (priv->scroll_to_path);
      priv->scroll_to_path = NULL;

      /* check if the reference was still valid */
      if (G_LIKELY (path != NULL))
        {
          /* try to scroll again */
          blxo_icon_view_scroll_to_path (icon_view, path,
                                        priv->scroll_to_use_align,
                                        priv->scroll_to_row_align,
                                        priv->scroll_to_col_align);

          /* release the path */
          gtk_tree_path_free (path);
        }
    }

  /* check if we need to draw a drag indicator */
  blxo_icon_view_get_drag_dest_item (icon_view, &path, &dest_pos);
  if (G_UNLIKELY (path != NULL))
    {
      dest_index = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);
    }

  /* paint all items that are affected by the expose event */
  for (lp = priv->items; lp != NULL; lp = lp->next)
    {
      item = BLXO_ICON_VIEW_ITEM (lp->data);

      /* FIXME: padding? */
      paint_area.x      = item->area.x;
      paint_area.y      = item->area.y;
      paint_area.width  = item->area.width;
      paint_area.height = item->area.height;

      /* check whether we are clipped fully */
      if (!gdk_rectangle_intersect (&paint_area, &clip, NULL))
        continue;

      /* paint the item */
      blxo_icon_view_paint_item (icon_view, item, cr, item->area.x, item->area.y, TRUE);
      if (G_UNLIKELY (dest_index >= 0 && dest_item == NULL))
        {
          if (dest_index == g_list_index (priv->items, item))
            dest_item = item;
        }
    }

  /* draw the drag indicator */
  if (G_UNLIKELY (dest_item != NULL))
    {
      switch (dest_pos)
        {
        case BLXO_ICON_VIEW_DROP_INTO:
          gtk_render_focus (context,
                            cr,
                            dest_item->area.x, dest_item->area.y,
                            dest_item->area.width, dest_item->area.height);
          break;

        case BLXO_ICON_VIEW_DROP_ABOVE:
          gtk_render_focus (context,
                            cr,
                            dest_item->area.x, dest_item->area.y - 1,
                            dest_item->area.width, 2);
          break;

        case BLXO_ICON_VIEW_DROP_LEFT:
          gtk_render_focus (context,
                            cr,
                            dest_item->area.x - 1, dest_item->area.y,
                            2, dest_item->area.height);
          break;

        case BLXO_ICON_VIEW_DROP_BELOW:
          gtk_render_focus (context,
                            cr,
                            dest_item->area.x, dest_item->area.y + dest_item->area.height - 1,
                            dest_item->area.width, 2);
          break;

        case BLXO_ICON_VIEW_DROP_RIGHT:
          gtk_render_focus (context,
                            cr,
                            dest_item->area.x + dest_item->area.width - 1, dest_item->area.y,
                            2, dest_item->area.height);

        case BLXO_ICON_VIEW_NO_DROP:
          break;

        default:
          g_assert_not_reached ();
        }
    }

  /* draw the rubberband border */
  if (G_UNLIKELY (priv->doing_rubberband))
    {
      cairo_save (cr);

      rect.x = MIN (icon_view->priv->rubberband_x_1, icon_view->priv->rubberband_x2);
      rect.y = MIN (icon_view->priv->rubberband_y_1, icon_view->priv->rubberband_y2);
      rect.width = ABS (icon_view->priv->rubberband_x_1 - icon_view->priv->rubberband_x2) + 1;
      rect.height = ABS (icon_view->priv->rubberband_y_1 - icon_view->priv->rubberband_y2) + 1;

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RUBBERBAND);

      gdk_cairo_rectangle (cr, &rect);
      cairo_clip (cr);

      gtk_render_background (context, cr,
                             rect.x, rect.y,
                             rect.width, rect.height);
      gtk_render_frame (context, cr,
                        rect.x, rect.y,
                        rect.width, rect.height);

      gtk_style_context_restore (context);
      cairo_restore (cr);
    }

  /* let the GtkContainer forward the draw event to all children */
  GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->draw (widget, cr);

  return FALSE;
}

#else

static gboolean
blxo_icon_view_expose_event (GtkWidget      *widget,
                            GdkEventExpose *event)
{
  BlxoIconViewDropPosition dest_pos;
  BlxoIconViewPrivate     *priv = BLXO_ICON_VIEW (widget)->priv;
  BlxoIconViewItem        *dest_item = NULL;
  BlxoIconViewItem        *item;
  GdkRectangle            event_area = event->area;
  BlxoIconView            *icon_view = BLXO_ICON_VIEW (widget);
  GtkTreePath            *path;
  GdkRectangle            rubber_rect;
  GdkRectangle            rect;
  const GList            *lp;
  gint                    event_area_last;
  gint                    dest_index = -1;
  cairo_t                *cr;
  GtkStyle               *style;

  /* verify that the expose happened on the icon window */
  if (G_UNLIKELY (event->window != priv->bin_window))
    return FALSE;

  /* don't handle expose if the layout isn't done yet; the layout
   * method will schedule a redraw when done.
   */
  if (G_UNLIKELY (priv->layout_idle_id != 0))
    return FALSE;

  /* scroll to the previously remembered path (if any) */
  if (G_UNLIKELY (priv->scroll_to_path != NULL))
    {
      /* grab the path from the reference and invalidate the reference */
      path = gtk_tree_row_reference_get_path (priv->scroll_to_path);
      gtk_tree_row_reference_free (priv->scroll_to_path);
      priv->scroll_to_path = NULL;

      /* check if the reference was still valid */
      if (G_LIKELY (path != NULL))
        {
          /* try to scroll again */
          blxo_icon_view_scroll_to_path (icon_view, path,
                                        priv->scroll_to_use_align,
                                        priv->scroll_to_row_align,
                                        priv->scroll_to_col_align);

          /* release the path */
          gtk_tree_path_free (path);
        }
    }

  /* check if we need to draw a drag indicator */
  blxo_icon_view_get_drag_dest_item (icon_view, &path, &dest_pos);
  if (G_UNLIKELY (path != NULL))
    {
      dest_index = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);
    }

  /* determine the last interesting coordinate (depending on the layout mode) */
  event_area_last = (priv->layout_mode == BLXO_ICON_VIEW_LAYOUT_ROWS)
                  ? event_area.y + event_area.height
                  : event_area.x + event_area.width;

  /* paint all items that are affected by the expose event */
  for (lp = priv->items; lp != NULL; lp = lp->next)
    {
      /* check if this item is in the visible area */
      item = BLXO_ICON_VIEW_ITEM (lp->data);
      if (G_LIKELY (priv->layout_mode == BLXO_ICON_VIEW_LAYOUT_ROWS))
        {
          if (item->area.y > event_area_last)
            break;
          else if (item->area.y + item->area.height < event_area.y)
            continue;
        }
      else
        {
          if (item->area.x > event_area_last)
            break;
          else if (item->area.x + item->area.width < event_area.x)
            continue;
        }

      /* check if this item needs an update */
      if (G_LIKELY (gdk_region_rect_in (event->region, &item->area) != GDK_OVERLAP_RECTANGLE_OUT))
        {
          blxo_icon_view_paint_item (icon_view, item, &event_area, event->window, item->area.x, item->area.y, TRUE);
          if (G_UNLIKELY (dest_index >= 0 && dest_item == NULL)) {
           if (dest_index == g_list_index (priv->items, item))
            dest_item = item;}
        }
    }

  /* draw the drag indicator */
  if (G_UNLIKELY (dest_item != NULL))
    {
      switch (dest_pos)
        {
        case BLXO_ICON_VIEW_DROP_INTO:
          gtk_paint_focus (gtk_widget_get_style (widget), priv->bin_window,
                           gtk_widget_get_state (widget), NULL, widget,
                           "iconview-drop-indicator",
                           dest_item->area.x, dest_item->area.y,
                           dest_item->area.width, dest_item->area.height);
          break;

        case BLXO_ICON_VIEW_DROP_ABOVE:
          gtk_paint_focus (gtk_widget_get_style (widget), priv->bin_window,
                           gtk_widget_get_state (widget), NULL, widget,
                           "iconview-drop-indicator",
                           dest_item->area.x, dest_item->area.y - 1,
                           dest_item->area.width, 2);
          break;

        case BLXO_ICON_VIEW_DROP_LEFT:
          gtk_paint_focus (gtk_widget_get_style (widget), priv->bin_window,
                           gtk_widget_get_state (widget), NULL, widget,
                           "iconview-drop-indicator",
                           dest_item->area.x - 1, dest_item->area.y,
                           2, dest_item->area.height);
          break;

        case BLXO_ICON_VIEW_DROP_BELOW:
          gtk_paint_focus (gtk_widget_get_style (widget), priv->bin_window,
                           gtk_widget_get_state (widget), NULL, widget,
                           "iconview-drop-indicator",
                           dest_item->area.x, dest_item->area.y + dest_item->area.height - 1,
                           dest_item->area.width, 2);
          break;

        case BLXO_ICON_VIEW_DROP_RIGHT:
          gtk_paint_focus (gtk_widget_get_style (widget), priv->bin_window,
                           gtk_widget_get_state (widget), NULL, widget,
                           "iconview-drop-indicator",
                           dest_item->area.x + dest_item->area.width - 1, dest_item->area.y,
                           2, dest_item->area.height);

        case BLXO_ICON_VIEW_NO_DROP:
          break;

        default:
          g_assert_not_reached ();
        }
    }

  /* draw the rubberband border */
  if (G_UNLIKELY (priv->doing_rubberband))
    {
      /* calculate the rubberband area */
      rubber_rect.x = MIN (priv->rubberband_x_1, priv->rubberband_x2);
      rubber_rect.y = MIN (priv->rubberband_y_1, priv->rubberband_y2);
      rubber_rect.width = ABS (priv->rubberband_x_1 - priv->rubberband_x2) + 1;
      rubber_rect.height = ABS (priv->rubberband_y_1 - priv->rubberband_y2) + 1;

      if (gdk_rectangle_intersect (&rubber_rect, &event_area, &rect))
        {
          cr = gdk_cairo_create (GDK_DRAWABLE (event->window));
          cairo_set_line_width (cr, 1.0);
          style = gtk_widget_get_style (widget);

          /* draw the area */
          cairo_set_source_rgba (cr,
                                 style->fg[GTK_STATE_NORMAL].red / 65535.0,
                                 style->fg[GTK_STATE_NORMAL].green / 65535.0,
                                 style->fg[GTK_STATE_NORMAL].blue / 65535.0,
                                 0.25);
          gdk_cairo_rectangle (cr, &rect);
          cairo_clip (cr);
          cairo_paint (cr);

          /* draw the border */
          cairo_set_source_rgb (cr,
                                style->fg[GTK_STATE_NORMAL].red / 65535.0,
                                style->fg[GTK_STATE_NORMAL].green / 65535.0,
                                style->fg[GTK_STATE_NORMAL].blue / 65535.0);
          cairo_rectangle (cr,
                          rubber_rect.x + 0.5, rubber_rect.y + 0.5,
                          rubber_rect.width - 1, rubber_rect.height - 1);
          cairo_stroke (cr);
          cairo_destroy (cr);
        }
    }

  /* let the GtkContainer forward the expose event to all children */
  (*GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->expose_event) (widget, event);

  return FALSE;
}

#endif


static gboolean
rubberband_scroll_timeout (gpointer user_data)
{
  GtkAdjustment *adjustment;
  BlxoIconView   *icon_view = BLXO_ICON_VIEW (user_data);
  gdouble        value;

  /* determine the adjustment for the scroll direction */
  adjustment = (icon_view->priv->layout_mode == BLXO_ICON_VIEW_LAYOUT_ROWS)
             ? icon_view->priv->vadjustment
             : icon_view->priv->hadjustment;

  /* determine the new scroll value */
  value = MIN (gtk_adjustment_get_value (adjustment) + icon_view->priv->scroll_value_diff, gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment));

  /* apply the new value */
  gtk_adjustment_set_value (adjustment, value);

  /* update the rubberband */
  blxo_icon_view_update_rubberband (icon_view);

  return TRUE;
}


static gboolean
blxo_icon_view_motion_notify_event (GtkWidget      *widget,
                                   GdkEventMotion *event)
{
  BlxoIconViewItem *item;
  BlxoIconView     *icon_view = BLXO_ICON_VIEW (widget);
  GdkCursor       *cursor;
  gint             size;
  gint             abso;
  GtkAllocation    allocation;

  blxo_icon_view_maybe_begin_drag (icon_view, event);
  gtk_widget_get_allocation (widget, &allocation);

  if (icon_view->priv->doing_rubberband)
    {
      blxo_icon_view_update_rubberband (widget);

      if (icon_view->priv->layout_mode == BLXO_ICON_VIEW_LAYOUT_ROWS)
        {
          abso = event->y - icon_view->priv->height *
             (gtk_adjustment_get_value (icon_view->priv->vadjustment) /
             (gtk_adjustment_get_upper (icon_view->priv->vadjustment) -
              gtk_adjustment_get_lower (icon_view->priv->vadjustment)));

          size = allocation.height;
        }
      else
        {
          abso = event->x - icon_view->priv->width *
             (gtk_adjustment_get_value (icon_view->priv->hadjustment) /
             (gtk_adjustment_get_upper (icon_view->priv->hadjustment) -
              gtk_adjustment_get_lower (icon_view->priv->hadjustment)));

          size = allocation.width;
        }

      if (abso < 0 || abso > size)
        {
          if (abso < 0)
            icon_view->priv->scroll_value_diff = abso;
          else
            icon_view->priv->scroll_value_diff = abso - size;
          icon_view->priv->event_last_x = event->x;
          icon_view->priv->event_last_y = event->y;

          if (icon_view->priv->scroll_timeout_id == 0)
            icon_view->priv->scroll_timeout_id = gdk_threads_add_timeout (30, rubberband_scroll_timeout,
                                                                          icon_view);
        }
      else
        {
          remove_scroll_timeout (icon_view);
        }
    }
  else
    {
      item = blxo_icon_view_get_item_at_coords (icon_view, event->x, event->y, TRUE, NULL);
      if (item != icon_view->priv->prelit_item)
        {
          if (G_LIKELY (icon_view->priv->prelit_item != NULL))
            blxo_icon_view_queue_draw_item (icon_view, icon_view->priv->prelit_item);
          icon_view->priv->prelit_item = item;
          if (G_LIKELY (item != NULL))
            blxo_icon_view_queue_draw_item (icon_view, item);

          /* check if we are in single click mode right now */
          if (G_UNLIKELY (icon_view->priv->single_click))
            {
              /* display a hand cursor when pointer is above an item */
              if (G_LIKELY (item != NULL))
                {
                  /* hand2 seems to be what we should use */
                  cursor = gdk_cursor_new_for_display (gdk_window_get_display (event->window), GDK_HAND2);
                  gdk_window_set_cursor (event->window, cursor);
                  gdk_cursor_unref (cursor);
                }
              else
                {
                  /* reset the cursor */
                  gdk_window_set_cursor (event->window, NULL);
                }

              /* check if autoselection is enabled */
              if (G_LIKELY (icon_view->priv->single_click_timeout > 0))
                {
                  /* drop any running timeout */
                  if (G_LIKELY (icon_view->priv->single_click_timeout_id != 0))
                    g_source_remove (icon_view->priv->single_click_timeout_id);

                  /* remember the current event state */
                  icon_view->priv->single_click_timeout_state = event->state;

                  /* schedule a new timeout */
                  icon_view->priv->single_click_timeout_id = gdk_threads_add_timeout_full (G_PRIORITY_LOW, icon_view->priv->single_click_timeout,
                                                                                 blxo_icon_view_single_click_timeout, icon_view,
                                                                                 blxo_icon_view_single_click_timeout_destroy);
                }
            }
        }
    }

  return TRUE;
}



static void
blxo_icon_view_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
  BlxoIconViewChild *child;
  BlxoIconView      *icon_view = BLXO_ICON_VIEW (container);
  GList            *lp;

  for (lp = icon_view->priv->children; lp != NULL; lp = lp->next)
    {
      child = lp->data;
      if (G_LIKELY (child->widget == widget))
        {
          icon_view->priv->children = g_list_delete_link (icon_view->priv->children, lp);
          gtk_widget_unparent (widget);
          g_slice_free (BlxoIconViewChild, child);
          return;
        }
    }
}



static void
blxo_icon_view_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (container);
  GList       *lp;

  for (lp = icon_view->priv->children; lp != NULL; lp = lp->next)
    (*callback) (((BlxoIconViewChild *) lp->data)->widget, callback_data);
}



static void
blxo_icon_view_item_activate_cell (BlxoIconView         *icon_view,
                                  BlxoIconViewItem     *item,
                                  BlxoIconViewCellInfo *info,
                                  GdkEvent            *event)
{
  GtkCellRendererMode mode;
  GdkRectangle        cell_area;
  GtkTreePath        *path;
  gboolean            visible;
  gchar              *path_string;

  blxo_icon_view_set_cell_data (icon_view, item);

  g_object_get (G_OBJECT (info->cell), "visible", &visible, "mode", &mode, NULL);

  if (G_UNLIKELY (visible && mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE))
    {
      blxo_icon_view_get_cell_area (icon_view, item, info, &cell_area);

      path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1);
      path_string = gtk_tree_path_to_string (path);
      gtk_tree_path_free (path);

      gtk_cell_renderer_activate (info->cell, event, GTK_WIDGET (icon_view), path_string, &cell_area, &cell_area, 0);

      g_free (path_string);
    }
}



static void
blxo_icon_view_put (BlxoIconView     *icon_view,
                   GtkWidget       *widget,
                   BlxoIconViewItem *item,
                   gint             cell)
{
  BlxoIconViewChild *child;

  /* allocate the new child */
  child = g_slice_new (BlxoIconViewChild);
  child->widget = widget;
  child->item = item;
  child->cell = cell;

  /* hook up the child */
  icon_view->priv->children = g_list_append (icon_view->priv->children, child);

  /* setup the parent for the child */
  if (gtk_widget_get_realized (GTK_WIDGET (icon_view)))
    gtk_widget_set_parent_window (child->widget, icon_view->priv->bin_window);
  gtk_widget_set_parent (widget, GTK_WIDGET (icon_view));
}



static void
blxo_icon_view_remove_widget (GtkCellEditable *editable,
                             BlxoIconView     *icon_view)
{
  BlxoIconViewItem *item;
  GList           *lp;

  if (G_LIKELY (icon_view->priv->edited_item != NULL))
    {
      item = icon_view->priv->edited_item;
      icon_view->priv->edited_item = NULL;
      icon_view->priv->editable = NULL;

      for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
        ((BlxoIconViewCellInfo *) lp->data)->editing = FALSE;

      if (gtk_widget_has_focus (GTK_WIDGET (editable)))
        gtk_widget_grab_focus (GTK_WIDGET (icon_view));

      g_signal_handlers_disconnect_by_func (editable, blxo_icon_view_remove_widget, icon_view);
      gtk_container_remove (GTK_CONTAINER (icon_view), GTK_WIDGET (editable));

      blxo_icon_view_queue_draw_item (icon_view, item);
    }
}



static void
blxo_icon_view_start_editing (BlxoIconView         *icon_view,
                             BlxoIconViewItem     *item,
                             BlxoIconViewCellInfo *info,
                             GdkEvent            *event)
{
  GtkCellRendererMode mode;
  GtkCellEditable    *editable;
  GdkRectangle        cell_area;
  GtkTreePath        *path;
  gboolean            visible;
  gchar              *path_string;

  /* setup cell data for the given item */
  blxo_icon_view_set_cell_data (icon_view, item);

  /* check if the cell is visible and editable (given the updated cell data) */
  g_object_get (info->cell, "visible", &visible, "mode", &mode, NULL);
  if (G_LIKELY (visible && mode == GTK_CELL_RENDERER_MODE_EDITABLE))
    {
      /* draw keyboard focus while editing */
      BLXO_ICON_VIEW_SET_FLAG (icon_view, BLXO_ICON_VIEW_DRAW_KEYFOCUS);

      /* determine the cell area */
      blxo_icon_view_get_cell_area (icon_view, item, info, &cell_area);

      /* determine the tree path */
      path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1);
      path_string = gtk_tree_path_to_string (path);
      gtk_tree_path_free (path);

      /* allocate the editable from the cell renderer */
      editable = gtk_cell_renderer_start_editing (info->cell, event, GTK_WIDGET (icon_view), path_string, &cell_area, &cell_area, 0);

      /* ugly hack, but works */
      if (g_object_class_find_property (G_OBJECT_GET_CLASS (editable), "has-frame") != NULL)
        g_object_set (editable, "has-frame", TRUE, NULL);

      /* setup the editing widget */
      icon_view->priv->edited_item = item;
      icon_view->priv->editable = editable;
      info->editing = TRUE;

      blxo_icon_view_put (icon_view, GTK_WIDGET (editable), item, info->position);
      gtk_cell_editable_start_editing (GTK_CELL_EDITABLE (editable), (GdkEvent *)event);
      gtk_widget_grab_focus (GTK_WIDGET (editable));
      g_signal_connect (G_OBJECT (editable), "remove-widget", G_CALLBACK (blxo_icon_view_remove_widget), icon_view);

      /* cleanup */
      g_free (path_string);
    }
}



static void
blxo_icon_view_stop_editing (BlxoIconView *icon_view,
                            gboolean     cancel_editing)
{
  BlxoIconViewItem *item;
  GtkCellRenderer *cell = NULL;
  GList           *lp;

  if (icon_view->priv->edited_item == NULL)
    return;

  /*
   * This is very evil. We need to do this, because
   * gtk_cell_editable_editing_done may trigger blxo_icon_view_row_changed
   * later on. If blxo_icon_view_row_changed notices
   * icon_view->priv->edited_item != NULL, it'll call
   * blxo_icon_view_stop_editing again. Bad things will happen then.
   *
   * Please read that again if you intend to modify anything here.
   */

  item = icon_view->priv->edited_item;
  icon_view->priv->edited_item = NULL;

  for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
    {
      BlxoIconViewCellInfo *info = lp->data;
      if (info->editing)
        {
          cell = info->cell;
          break;
        }
    }

  if (G_UNLIKELY (cell == NULL))
    return;

  gtk_cell_renderer_stop_editing (cell, cancel_editing);
  if (G_LIKELY (!cancel_editing))
    gtk_cell_editable_editing_done (icon_view->priv->editable);

  icon_view->priv->edited_item = item;

  gtk_cell_editable_remove_widget (icon_view->priv->editable);
}



static gboolean
blxo_icon_view_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  BlxoIconViewCellInfo *info = NULL;
  GtkCellRendererMode  mode;
  BlxoIconViewItem     *item;
  BlxoIconView         *icon_view;
  GtkTreePath         *path;
  gboolean             dirty = FALSE;
  gint                 cursor_cell;

  icon_view = BLXO_ICON_VIEW (widget);

  if (event->window != icon_view->priv->bin_window)
    return FALSE;

  /* stop any pending "single-click-timeout" */
  if (G_UNLIKELY (icon_view->priv->single_click_timeout_id != 0))
    g_source_remove (icon_view->priv->single_click_timeout_id);

  if (G_UNLIKELY (!gtk_widget_has_focus (widget)))
    gtk_widget_grab_focus (widget);

  if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
      item = blxo_icon_view_get_item_at_coords (icon_view,
                                               event->x, event->y,
                                               TRUE,
                                               &info);
      if (item != NULL)
        {
          g_object_get (info->cell, "mode", &mode, NULL);

          if (mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE ||
              mode == GTK_CELL_RENDERER_MODE_EDITABLE)
            cursor_cell = g_list_index (icon_view->priv->cell_list, info);
          else
            cursor_cell = -1;

          blxo_icon_view_scroll_to_item (icon_view, item);

          if (icon_view->priv->selection_mode == GTK_SELECTION_NONE)
            {
              blxo_icon_view_set_cursor_item (icon_view, item, cursor_cell);
            }
          else if (icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE &&
                   (event->state & GDK_SHIFT_MASK))
            {
              if (!(event->state & GDK_CONTROL_MASK))
                blxo_icon_view_unselect_all_internal (icon_view);

              blxo_icon_view_set_cursor_item (icon_view, item, cursor_cell);
              if (!icon_view->priv->anchor_item)
                icon_view->priv->anchor_item = item;
              else
                blxo_icon_view_select_all_between (icon_view,
                                                  icon_view->priv->anchor_item,
                                                  item);
              dirty = TRUE;
            }
          else
            {
              if ((icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE ||
                  ((icon_view->priv->selection_mode == GTK_SELECTION_SINGLE) && item->selected)) &&
                  (event->state & GDK_CONTROL_MASK))
                {
                  item->selected = !item->selected;
                  blxo_icon_view_queue_draw_item (icon_view, item);
                  dirty = TRUE;
                }
              else
                {
                  if (!item->selected)
                    {
                      blxo_icon_view_unselect_all_internal (icon_view);

                      item->selected = TRUE;
                      blxo_icon_view_queue_draw_item (icon_view, item);
                      dirty = TRUE;
                    }
                }
              blxo_icon_view_set_cursor_item (icon_view, item, cursor_cell);
              icon_view->priv->anchor_item = item;
            }

          /* Save press to possibly begin a drag */
          if (icon_view->priv->pressed_button < 0)
            {
              icon_view->priv->pressed_button = event->button;
              icon_view->priv->press_start_x = event->x;
              icon_view->priv->press_start_y = event->y;
            }

          if (G_LIKELY (icon_view->priv->last_single_clicked == NULL))
            icon_view->priv->last_single_clicked = item;

          /* cancel the current editing, if it exists */
          blxo_icon_view_stop_editing (icon_view, TRUE);

          if (mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE)
            blxo_icon_view_item_activate_cell (icon_view, item, info,
                                              (GdkEvent *)event);
          else if (mode == GTK_CELL_RENDERER_MODE_EDITABLE)
            blxo_icon_view_start_editing (icon_view, item, info,
                                         (GdkEvent *)event);
        }
      else
        {
          /* cancel the current editing, if it exists */
          blxo_icon_view_stop_editing (icon_view, TRUE);

          if (icon_view->priv->selection_mode != GTK_SELECTION_BROWSE &&
              !(event->state & GDK_CONTROL_MASK))
            {
              dirty = blxo_icon_view_unselect_all_internal (icon_view);
            }

          if (icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE)
            blxo_icon_view_start_rubberbanding (icon_view, event->x, event->y);
        }
    }
  else if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
    {
      /* ignore double-click events in single-click mode */
      if (G_LIKELY (!icon_view->priv->single_click))
        {
          item = blxo_icon_view_get_item_at_coords (icon_view,
                                                   event->x, event->y,
                                                   TRUE,
                                                   NULL);
          if (G_LIKELY (item != NULL))
            {
              path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1);
              blxo_icon_view_item_activated (icon_view, path);
              gtk_tree_path_free (path);
            }
        }

      icon_view->priv->last_single_clicked = NULL;
      icon_view->priv->pressed_button = -1;
    }

  /* grab focus and stop drawing the keyboard focus indicator on single clicks */
  if (G_LIKELY (event->type != GDK_2BUTTON_PRESS && event->type != GDK_3BUTTON_PRESS))
    {
      if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
        gtk_widget_grab_focus (GTK_WIDGET (icon_view));
      BLXO_ICON_VIEW_UNSET_FLAG (icon_view, BLXO_ICON_VIEW_DRAW_KEYFOCUS);
    }

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);

  return event->button == 1;
}



static gboolean
blxo_icon_view_button_release_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  BlxoIconViewItem *item;
  BlxoIconView     *icon_view = BLXO_ICON_VIEW (widget);
  GtkTreePath     *path;

  if (icon_view->priv->pressed_button == (gint) event->button)
    {
      /* check if we're in single click mode */
      if (G_UNLIKELY (icon_view->priv->single_click && (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == 0))
        {
          /* determine the item at the mouse coords and check if this is the last single clicked one */
          item = blxo_icon_view_get_item_at_coords (icon_view, event->x, event->y, TRUE, NULL);
          if (G_LIKELY (item != NULL && item == icon_view->priv->last_single_clicked))
            {
              /* emit an "item-activated" signal for this item */
              path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1);
              blxo_icon_view_item_activated (icon_view, path);
              gtk_tree_path_free (path);
            }

          /* reset the last single clicked item */
          icon_view->priv->last_single_clicked = NULL;
        }

      /* reset the pressed_button state */
      icon_view->priv->pressed_button = -1;
    }

  blxo_icon_view_stop_rubberbanding (icon_view);

  remove_scroll_timeout (icon_view);

  return TRUE;
}



static gboolean
blxo_icon_view_scroll_event (GtkWidget      *widget,
                            GdkEventScroll *event)
{
  GtkAdjustment *adjustment;
  BlxoIconView   *icon_view = BLXO_ICON_VIEW (widget);
  gdouble        delta;
  gdouble        value;

  /* we don't care for scroll events in "rows" layout mode, as
   * that's completely handled by GtkScrolledWindow.
   */
  if (icon_view->priv->layout_mode != BLXO_ICON_VIEW_LAYOUT_COLS)
    return FALSE;

  /* also, we don't care for anything but Up/Down, as
   * everything else will be handled by GtkScrolledWindow.
   */
  if (event->direction != GDK_SCROLL_UP && event->direction != GDK_SCROLL_DOWN)
    return FALSE;

  /* determine the horizontal adjustment */
  adjustment = icon_view->priv->hadjustment;

  /* determine the scroll delta */
  delta = pow (gtk_adjustment_get_page_size (adjustment), 2.0 / 3.0);
  delta = (event->direction == GDK_SCROLL_UP) ? -delta : delta;

  /* apply the new adjustment value */
  value = CLAMP (gtk_adjustment_get_value (adjustment) + delta, gtk_adjustment_get_lower (adjustment), gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment));
  gtk_adjustment_set_value (adjustment, value);

  return TRUE;
}



static gboolean
blxo_icon_view_key_press_event (GtkWidget   *widget,
                               GdkEventKey *event)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (widget);
  gboolean     retval;
#if !GTK_CHECK_VERSION (3,16,0)
  GTypeClass *klass;
#endif

  /* let the parent class handle the key bindings and stuff */
  if ((*GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->key_press_event) (widget, event))
    return TRUE;

  /* check if typeahead search is enabled */
  if (G_UNLIKELY (!icon_view->priv->enable_search))
    return FALSE;

  blxo_icon_view_search_ensure_directory (icon_view);

  /* check if keypress results in a text change in search_entry; prevents showing the search
   * window when only modifier keys (shift, control, ...) are pressed */
  retval = gtk_entry_im_context_filter_keypress (GTK_ENTRY (icon_view->priv->search_entry), event);

  if (retval)
    {
      if (blxo_icon_view_search_start (icon_view, FALSE))
        {
#if GTK_CHECK_VERSION (3,16,0)
          gtk_entry_grab_focus_without_selecting (GTK_ENTRY (icon_view->priv->search_entry));
#else
          klass = g_type_class_peek_parent (GTK_ENTRY_GET_CLASS (icon_view->priv->search_entry));
          (*GTK_WIDGET_CLASS (klass)->grab_focus) (icon_view->priv->search_entry);
#endif
          return TRUE;
        }
      else
        {
          gtk_entry_set_text (GTK_ENTRY (icon_view->priv->search_entry), "");
          return FALSE;
        }
    }

  return FALSE;
}



static gboolean
blxo_icon_view_focus_out_event (GtkWidget     *widget,
                               GdkEventFocus *event)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (widget);

  /* be sure to cancel any single-click timeout */
  if (G_UNLIKELY (icon_view->priv->single_click_timeout_id != 0))
    g_source_remove (icon_view->priv->single_click_timeout_id);

  /* reset the cursor if we're still realized */
  if (G_LIKELY (icon_view->priv->bin_window != NULL))
    gdk_window_set_cursor (icon_view->priv->bin_window, NULL);

  /* destroy the interactive search dialog */
  if (G_UNLIKELY (icon_view->priv->search_window != NULL))
    blxo_icon_view_search_dialog_hide (icon_view->priv->search_window, icon_view);

  /* schedule a redraw with the new focus state */
  gtk_widget_queue_draw (widget);

  return FALSE;
}



static gboolean
blxo_icon_view_leave_notify_event (GtkWidget        *widget,
                                  GdkEventCrossing *event)
{
  BlxoIconView         *icon_view = BLXO_ICON_VIEW (widget);

  /* reset cursor to default */
  if (gtk_widget_get_realized (widget))
    gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);

  /* reset the prelit item (if any) */
  if (G_LIKELY (icon_view->priv->prelit_item != NULL))
    {
      blxo_icon_view_queue_draw_item (icon_view, icon_view->priv->prelit_item);
      icon_view->priv->prelit_item = NULL;
    }

  /* call the parent's leave_notify_event (if any) */
  if (GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->leave_notify_event != NULL)
    return (*GTK_WIDGET_CLASS (blxo_icon_view_parent_class)->leave_notify_event) (widget, event);

  /* other signal handlers may be invoked */
  return FALSE;
}



static void
blxo_icon_view_update_rubberband (gpointer data)
{
  BlxoIconView *icon_view;
  gint x, y;
  GdkRectangle old_area;
  GdkRectangle new_area;
  GdkRectangle common;
  GdkRegion *invalid_region;
#if GTK_CHECK_VERSION (3, 16, 0)
  GdkSeat *seat;
  GdkDevice        *pointer_dev;
#endif

  icon_view = BLXO_ICON_VIEW (data);

#if GTK_CHECK_VERSION (3, 16, 0)
  seat = gdk_display_get_default_seat (gdk_window_get_display (icon_view->priv->bin_window));
  pointer_dev = gdk_seat_get_pointer (seat);
  gdk_window_get_device_position (icon_view->priv->bin_window, pointer_dev, &x, &y, NULL);
#else
  gdk_window_get_pointer (icon_view->priv->bin_window, &x, &y, NULL);
#endif

  x = MAX (x, 0);
  y = MAX (y, 0);

  old_area.x = MIN (icon_view->priv->rubberband_x_1,
                    icon_view->priv->rubberband_x2);
  old_area.y = MIN (icon_view->priv->rubberband_y_1,
                    icon_view->priv->rubberband_y2);
  old_area.width = ABS (icon_view->priv->rubberband_x2 -
                        icon_view->priv->rubberband_x_1) + 1;
  old_area.height = ABS (icon_view->priv->rubberband_y2 -
                         icon_view->priv->rubberband_y_1) + 1;

  new_area.x = MIN (icon_view->priv->rubberband_x_1, x);
  new_area.y = MIN (icon_view->priv->rubberband_y_1, y);
  new_area.width = ABS (x - icon_view->priv->rubberband_x_1) + 1;
  new_area.height = ABS (y - icon_view->priv->rubberband_y_1) + 1;

  invalid_region = gdk_region_rectangle (&old_area);
  gdk_region_union_with_rect (invalid_region, &new_area);

  gdk_rectangle_intersect (&old_area, &new_area, &common);
  if (common.width > 2 && common.height > 2)
    {
      GdkRegion *common_region;

      /* make sure the border is invalidated */
      common.x += 1;
      common.y += 1;
      common.width -= 2;
      common.height -= 2;

      common_region = gdk_region_rectangle (&common);

      gdk_region_subtract (invalid_region, common_region);
      gdk_region_destroy (common_region);
    }

  gdk_window_invalidate_region (icon_view->priv->bin_window, invalid_region, TRUE);

  gdk_region_destroy (invalid_region);

  icon_view->priv->rubberband_x2 = x;
  icon_view->priv->rubberband_y2 = y;

  blxo_icon_view_update_rubberband_selection (icon_view);
}



static void
blxo_icon_view_start_rubberbanding (BlxoIconView  *icon_view,
                                   gint          x,
                                   gint          y)
{
  gpointer  drag_data;
  GList    *items;

  /* be sure to disable any previously active rubberband */
  blxo_icon_view_stop_rubberbanding (icon_view);

  for (items = icon_view->priv->items; items; items = items->next)
    {
      BlxoIconViewItem *item = items->data;
      item->selected_before_rubberbanding = item->selected;
    }

  icon_view->priv->rubberband_x_1 = x;
  icon_view->priv->rubberband_y_1 = y;
  icon_view->priv->rubberband_x2 = x;
  icon_view->priv->rubberband_y2 = y;

  icon_view->priv->doing_rubberband = TRUE;

  gtk_grab_add (GTK_WIDGET (icon_view));

  /* be sure to disable Gtk+ DnD callbacks, because else rubberbanding will be interrupted */
  drag_data = g_object_get_data (G_OBJECT (icon_view), I_("gtk-site-data"));
  if (G_LIKELY (drag_data != NULL))
    {
      g_signal_handlers_block_matched (G_OBJECT (icon_view),
                                       G_SIGNAL_MATCH_DATA,
                                       0, 0, NULL, NULL,
                                       drag_data);
    }
}



static void
blxo_icon_view_stop_rubberbanding (BlxoIconView *icon_view)
{
  gpointer drag_data;

  if (G_LIKELY (icon_view->priv->doing_rubberband))
    {
      icon_view->priv->doing_rubberband = FALSE;
      gtk_grab_remove (GTK_WIDGET (icon_view));
      gtk_widget_queue_draw (GTK_WIDGET (icon_view));

      /* re-enable Gtk+ DnD callbacks again */
      drag_data = g_object_get_data (G_OBJECT (icon_view), I_("gtk-site-data"));
      if (G_LIKELY (drag_data != NULL))
        {
          g_signal_handlers_unblock_matched (G_OBJECT (icon_view),
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             drag_data);
        }
    }
}



static void
blxo_icon_view_update_rubberband_selection (BlxoIconView *icon_view)
{
  BlxoIconViewItem *item;
  gboolean         selected;
  gboolean         changed = FALSE;
  gboolean         is_in;
  GList           *lp;
  gint             x, y;
  gint             width;
  gint             height;

  /* determine the new rubberband area */
  x = MIN (icon_view->priv->rubberband_x_1, icon_view->priv->rubberband_x2);
  y = MIN (icon_view->priv->rubberband_y_1, icon_view->priv->rubberband_y2);
  width = ABS (icon_view->priv->rubberband_x_1 - icon_view->priv->rubberband_x2);
  height = ABS (icon_view->priv->rubberband_y_1 - icon_view->priv->rubberband_y2);

  /* check all items */
  for (lp = icon_view->priv->items; lp != NULL; lp = lp->next)
    {
      item = BLXO_ICON_VIEW_ITEM (lp->data);

      is_in = blxo_icon_view_item_hit_test (icon_view, item, x, y, width, height);

      selected = is_in ^ item->selected_before_rubberbanding;

      if (G_UNLIKELY (item->selected != selected))
        {
          changed = TRUE;
          item->selected = selected;
          blxo_icon_view_queue_draw_item (icon_view, item);
        }
    }

  if (G_LIKELY (changed))
    g_signal_emit (G_OBJECT (icon_view), icon_view_signals[SELECTION_CHANGED], 0);
}



static gboolean
blxo_icon_view_item_hit_test (BlxoIconView      *icon_view,
                             BlxoIconViewItem  *item,
                             gint              x,
                             gint              y,
                             gint              width,
                             gint              height)
{
  GList               *l;
  GdkRectangle         box;
  BlxoIconViewCellInfo *info;

  for (l = icon_view->priv->cell_list; l; l = l->next)
    {
      info = l->data;

      if (!gtk_cell_renderer_get_visible (info->cell)
          || item->box == NULL)
        continue;

      box = item->box[info->position];

      if (MIN (x + width, box.x + box.width) - MAX (x, box.x) > 0
          && MIN (y + height, box.y + box.height) - MAX (y, box.y) > 0)
        return TRUE;
    }

  return FALSE;
}



static gboolean
blxo_icon_view_unselect_all_internal (BlxoIconView  *icon_view)
{
  BlxoIconViewItem *item;
  gboolean         dirty = FALSE;
  GList           *lp;

  if (G_LIKELY (icon_view->priv->selection_mode != GTK_SELECTION_NONE))
    {
      for (lp = icon_view->priv->items; lp != NULL; lp = lp->next)
        {
          item = BLXO_ICON_VIEW_ITEM (lp->data);
          if (item->selected)
            {
              dirty = TRUE;
              item->selected = FALSE;
              blxo_icon_view_queue_draw_item (icon_view, item);
            }
        }
    }

  return dirty;
}



static void
blxo_icon_view_set_adjustments (BlxoIconView   *icon_view,
                               GtkAdjustment *hadj,
                               GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

  if (hadj)
    _blxo_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadj)
    _blxo_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  if (icon_view->priv->hadjustment && (icon_view->priv->hadjustment != hadj))
    {
      g_signal_handlers_disconnect_matched (icon_view->priv->hadjustment, G_SIGNAL_MATCH_DATA,
                                           0, 0, NULL, NULL, icon_view);
      g_object_unref (icon_view->priv->hadjustment);
    }

  if (icon_view->priv->vadjustment && (icon_view->priv->vadjustment != vadj))
    {
      g_signal_handlers_disconnect_matched (icon_view->priv->vadjustment, G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, icon_view);
      g_object_unref (icon_view->priv->vadjustment);
    }

  if (icon_view->priv->hadjustment != hadj)
    {
      icon_view->priv->hadjustment = hadj;
      g_object_ref_sink (icon_view->priv->hadjustment);

      g_signal_connect (icon_view->priv->hadjustment, "value-changed",
                        G_CALLBACK (blxo_icon_view_adjustment_changed),
                        icon_view);
      need_adjust = TRUE;
    }

  if (icon_view->priv->vadjustment != vadj)
    {
      icon_view->priv->vadjustment = vadj;
      g_object_ref_sink (icon_view->priv->vadjustment);

      g_signal_connect (icon_view->priv->vadjustment, "value-changed",
                        G_CALLBACK (blxo_icon_view_adjustment_changed),
                        icon_view);
      need_adjust = TRUE;
    }

  if (need_adjust)
    blxo_icon_view_adjustment_changed (NULL, icon_view);
}



static void
blxo_icon_view_real_select_all (BlxoIconView *icon_view)
{
  blxo_icon_view_select_all (icon_view);
}



static void
blxo_icon_view_real_unselect_all (BlxoIconView *icon_view)
{
  blxo_icon_view_unselect_all (icon_view);
}



static void
blxo_icon_view_real_select_cursor_item (BlxoIconView *icon_view)
{
  blxo_icon_view_unselect_all (icon_view);

  if (icon_view->priv->cursor_item != NULL)
    blxo_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
}



static gboolean
blxo_icon_view_real_activate_cursor_item (BlxoIconView *icon_view)
{
  GtkTreePath *path;
  GtkCellRendererMode mode;
  BlxoIconViewCellInfo *info = NULL;

  if (!icon_view->priv->cursor_item)
    return FALSE;

  info = g_list_nth_data (icon_view->priv->cell_list,
                          icon_view->priv->cursor_cell);

  if (info)
    {
      g_object_get (info->cell, "mode", &mode, NULL);

      if (mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE)
        {
          blxo_icon_view_item_activate_cell (icon_view,
                                            icon_view->priv->cursor_item,
                                            info, NULL);
          return TRUE;
        }
      else if (mode == GTK_CELL_RENDERER_MODE_EDITABLE)
        {
          blxo_icon_view_start_editing (icon_view,
                                       icon_view->priv->cursor_item,
                                       info, NULL);
          return TRUE;
        }
    }

  path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, icon_view->priv->cursor_item), -1);
  blxo_icon_view_item_activated (icon_view, path);
  gtk_tree_path_free (path);

  return TRUE;
}



static gboolean
blxo_icon_view_real_start_interactive_search (BlxoIconView *icon_view)
{
  return blxo_icon_view_search_start (icon_view, TRUE);
}



static void
blxo_icon_view_real_toggle_cursor_item (BlxoIconView *icon_view)
{
  if (G_LIKELY (icon_view->priv->cursor_item != NULL))
    {
      switch (icon_view->priv->selection_mode)
        {
        case GTK_SELECTION_NONE:
          break;

        case GTK_SELECTION_BROWSE:
          blxo_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
          break;

        case GTK_SELECTION_SINGLE:
          if (icon_view->priv->cursor_item->selected)
            blxo_icon_view_unselect_item (icon_view, icon_view->priv->cursor_item);
          else
            blxo_icon_view_select_item (icon_view, icon_view->priv->cursor_item);
          break;

        case GTK_SELECTION_MULTIPLE:
          icon_view->priv->cursor_item->selected = !icon_view->priv->cursor_item->selected;
          g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
          blxo_icon_view_queue_draw_item (icon_view, icon_view->priv->cursor_item);
          break;

        default:
          g_assert_not_reached ();
        }
    }
}



static void
blxo_icon_view_adjustment_changed (GtkAdjustment *adjustment,
                                  BlxoIconView   *icon_view)
{
  if (gtk_widget_get_realized (GTK_WIDGET (icon_view)))
    {
      gdk_window_move (icon_view->priv->bin_window, -gtk_adjustment_get_value (icon_view->priv->hadjustment), -gtk_adjustment_get_value (icon_view->priv->vadjustment));

      if (G_UNLIKELY (icon_view->priv->doing_rubberband))
        blxo_icon_view_update_rubberband (GTK_WIDGET (icon_view));

#if !GTK_CHECK_VERSION (3, 22, 0)
      gdk_window_process_updates (icon_view->priv->bin_window, TRUE);
#endif
    }
}



static GList*
blxo_icon_view_layout_single_row (BlxoIconView *icon_view,
                                 GList       *first_item,
                                 gint         item_width,
                                 gint         row,
                                 gint        *y,
                                 gint        *maximum_width,
                                 gint         max_cols)
{
  BlxoIconViewPrivate *priv = icon_view->priv;
  BlxoIconViewItem    *item;
  gboolean            rtl;
  GList              *last_item;
  GList              *items = first_item;
  gint               *max_width;
  gint               *max_height;
  gint                focus_width;
  gint                current_width;
  gint                colspan;
  gint                col = 0;
  gint                x;
  gint                i;
  GtkAllocation       allocation;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (icon_view)) == GTK_TEXT_DIR_RTL);
  gtk_widget_get_allocation (GTK_WIDGET (icon_view), &allocation);

  max_width = g_newa (gint, priv->n_cells);
  max_height = g_newa (gint, priv->n_cells);
  for (i = priv->n_cells; --i >= 0; )
    {
      max_width[i] = 0;
      max_height[i] = 0;
    }

  gtk_widget_style_get (GTK_WIDGET (icon_view),
                        "focus-line-width", &focus_width,
                        NULL);

  x = priv->margin + focus_width;
  current_width = 2 * (priv->margin + focus_width);

  for (items = first_item; items != NULL; items = items->next)
    {
      item = BLXO_ICON_VIEW_ITEM (items->data);

      blxo_icon_view_calculate_item_size (icon_view, item);
      colspan = 1 + (item->area.width - 1) / (item_width + priv->column_spacing);

      item->area.width = colspan * item_width + (colspan - 1) * priv->column_spacing;

      current_width += item->area.width + priv->column_spacing + 2 * focus_width;

      if (G_LIKELY (items != first_item))
        {
          if ((priv->columns <= 0 && current_width > allocation.width) ||
              (priv->columns > 0 && col >= priv->columns) ||
              (max_cols > 0 && col >= max_cols))
            break;
        }

      item->area.y = *y + focus_width;
      item->area.x = rtl ? allocation.width - item->area.width - x : x;

      x = current_width - (priv->margin + focus_width);

      for (i = 0; i < priv->n_cells; i++)
        {
          max_width[i] = MAX (max_width[i], item->box[i].width);
          max_height[i] = MAX (max_height[i], item->box[i].height);
        }

      if (current_width > *maximum_width)
        *maximum_width = current_width;

      item->row = row;
      item->col = col;

      col += colspan;
    }

  last_item = items;

  /* Now go through the row again and align the icons */
  for (items = first_item; items != last_item; items = items->next)
    {
      item = BLXO_ICON_VIEW_ITEM (items->data);

      blxo_icon_view_calculate_item_size2 (icon_view, item, max_width, max_height);

      /* We may want to readjust the new y coordinate. */
      if (item->area.y + item->area.height + focus_width + priv->row_spacing > *y)
        *y = item->area.y + item->area.height + focus_width + priv->row_spacing;

      if (G_UNLIKELY (rtl))
        item->col = col - 1 - item->col;
    }

  return last_item;
}



static GList*
blxo_icon_view_layout_single_col (BlxoIconView *icon_view,
                                 GList       *first_item,
                                 gint         item_height,
                                 gint         col,
                                 gint        *x,
                                 gint        *maximum_height,
                                 gint         max_rows)
{
  BlxoIconViewPrivate *priv = icon_view->priv;
  BlxoIconViewItem    *item;
  GList              *items = first_item;
  GList              *last_item;
  gint               *max_width;
  gint               *max_height;
  gint                focus_width;
  gint                current_height;
  gint                rowspan;
  gint                row = 0;
  gint                y;
  gint                i;
  GtkAllocation       allocation;

  max_width = g_newa (gint, priv->n_cells);
  max_height = g_newa (gint, priv->n_cells);
  for (i = priv->n_cells; --i >= 0; )
    {
      max_width[i] = 0;
      max_height[i] = 0;
    }

  gtk_widget_style_get (GTK_WIDGET (icon_view),
                        "focus-line-width", &focus_width,
                        NULL);
  gtk_widget_get_allocation (GTK_WIDGET (icon_view), &allocation);

  y = priv->margin + focus_width;
  current_height = 2 * (priv->margin + focus_width);

  for (items = first_item; items != NULL; items = items->next)
    {
      item = BLXO_ICON_VIEW_ITEM (items->data);

      blxo_icon_view_calculate_item_size (icon_view, item);

      rowspan = 1 + (item->area.height - 1) / (item_height + priv->row_spacing);

      item->area.height = rowspan * item_height + (rowspan - 1) * priv->row_spacing;

      current_height += item->area.height + priv->row_spacing + 2 * focus_width;

      if (G_LIKELY (items != first_item))
        {
          if (current_height >= allocation.height ||
             (max_rows > 0 && row >= max_rows))
            break;
        }

      item->area.y = y + focus_width;
      item->area.x = *x;

      y = current_height - (priv->margin + focus_width);

      for (i = 0; i < priv->n_cells; i++)
        {
          max_width[i] = MAX (max_width[i], item->box[i].width);
          max_height[i] = MAX (max_height[i], item->box[i].height);
        }

      if (current_height > *maximum_height)
        *maximum_height = current_height;

      item->row = row;
      item->col = col;

      row += rowspan;
    }

  last_item = items;

  /* Now go through the column again and align the icons */
  for (items = first_item; items != last_item; items = items->next)
    {
      item = BLXO_ICON_VIEW_ITEM (items->data);

      blxo_icon_view_calculate_item_size2 (icon_view, item, max_width, max_height);

      /* We may want to readjust the new x coordinate. */
      if (item->area.x + item->area.width + focus_width + priv->column_spacing > *x)
        *x = item->area.x + item->area.width + focus_width + priv->column_spacing;
    }

  return last_item;
}



static void
blxo_icon_view_set_adjustment_upper (GtkAdjustment *adj,
                                    gdouble        upper)
{
  if (upper != gtk_adjustment_get_upper (adj))
    {
      gdouble min = MAX (0.0, upper - gtk_adjustment_get_page_size (adj));
#if !GTK_CHECK_VERSION (3, 18, 0)
      gboolean value_changed = FALSE;
#endif

      gtk_adjustment_set_upper (adj, upper);

      if (gtk_adjustment_get_value (adj) > min)
        {
          gtk_adjustment_set_value (adj, min);
#if !GTK_CHECK_VERSION (3, 18, 0)
          value_changed = TRUE;
#endif
        }

      /* Prior to GTK 3.18, we need to emit "changed" and "value-changed" ourselves */
#if !GTK_CHECK_VERSION (3, 18, 0)
      gtk_adjustment_changed (adj);

      if (value_changed)
        gtk_adjustment_value_changed (adj);
#endif
    }
}



static gint
blxo_icon_view_layout_cols (BlxoIconView *icon_view,
                           gint         item_height,
                           gint        *x,
                           gint        *maximum_height,
                           gint         max_rows)
{
  GList *icons = icon_view->priv->items;
  GList *items;
  gint   col = 0;
  gint   rows = 0;

  *x = icon_view->priv->margin;

  do
    {
      icons = blxo_icon_view_layout_single_col (icon_view, icons,
                                               item_height, col,
                                               x, maximum_height, max_rows);

      /* count the number of rows in the first column */
      if (G_UNLIKELY (col == 0))
        {
          for (items = icon_view->priv->items, rows = 0; items != icons; items = items->next, ++rows)
            ;
        }

      col++;
    }
  while (icons != NULL);

  *x += icon_view->priv->margin;
  icon_view->priv->cols = col;

  return rows;
}



static gint
blxo_icon_view_layout_rows (BlxoIconView *icon_view,
                           gint         item_width,
                           gint        *y,
                           gint        *maximum_width,
                           gint         max_cols)
{
  GList *icons = icon_view->priv->items;
  GList *items;
  gint   row = 0;
  gint   cols = 0;

  *y = icon_view->priv->margin;

  do
    {
      icons = blxo_icon_view_layout_single_row (icon_view, icons,
                                               item_width, row,
                                               y, maximum_width, max_cols);

      /* count the number of columns in the first row */
      if (G_UNLIKELY (row == 0))
        {
          for (items = icon_view->priv->items, cols = 0; items != icons; items = items->next, ++cols)
            ;
        }

      row++;
    }
  while (icons != NULL);

  *y += icon_view->priv->margin;
  icon_view->priv->rows = row;

  return cols;
}



static void
blxo_icon_view_layout (BlxoIconView *icon_view)
{
  BlxoIconViewPrivate *priv = icon_view->priv;
  BlxoIconViewItem    *item;
  GList              *icons;
  gint                maximum_height = 0;
  gint                maximum_width = 0;
  gint                item_height;
  gint                item_width;
  gint                rows, cols;
  gint                x, y;
  GtkAllocation       allocation;
  GtkRequisition      requisition;

  /* verify that we still have a valid model */
  if (G_UNLIKELY (priv->model == NULL))
    return;

  gtk_widget_get_allocation (GTK_WIDGET (icon_view), &allocation);

#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_widget_get_preferred_width (GTK_WIDGET (icon_view), NULL, &requisition.width);
  gtk_widget_get_preferred_height (GTK_WIDGET (icon_view), NULL, &requisition.height);
#else
  gtk_widget_get_requisition (GTK_WIDGET (icon_view), &requisition);
#endif

  /* determine the layout mode */
  if (G_LIKELY (priv->layout_mode == BLXO_ICON_VIEW_LAYOUT_ROWS))
    {
      /* calculate item sizes on-demand */
      item_width = priv->item_width;
      if (item_width < 0)
        {
          for (icons = priv->items; icons != NULL; icons = icons->next)
            {
              item = icons->data;
              blxo_icon_view_calculate_item_size (icon_view, item);
              item_width = MAX (item_width, item->area.width);
            }
        }

      cols = blxo_icon_view_layout_rows (icon_view, item_width, &y, &maximum_width, 0);

      /* If, by adding another column, we increase the height of the icon view, thus forcing a
       * vertical scrollbar to appear that would prevent the last column from being able to fit,
       * we need to relayout the icons with one less column.
       */
      if (cols == priv->cols + 1 && y > allocation.height &&
          priv->height <= allocation.height)
        {
          cols = blxo_icon_view_layout_rows (icon_view, item_width, &y, &maximum_width, priv->cols);
        }

      priv->width = maximum_width;
      priv->height = y;
      priv->cols = cols;
    }
  else
    {
      /* calculate item sizes on-demand */
      for (icons = priv->items, item_height = 0; icons != NULL; icons = icons->next)
        {
          item = icons->data;
          blxo_icon_view_calculate_item_size (icon_view, item);
          item_height = MAX (item_height, item->area.height);
        }

      rows = blxo_icon_view_layout_cols (icon_view, item_height, &x, &maximum_height, 0);

      /* If, by adding another row, we increase the width of the icon view, thus forcing a
       * horizontal scrollbar to appear that would prevent the last row from being able to fit,
       * we need to relayout the icons with one less row.
       */
      if (rows == priv->rows + 1 && x > allocation.width &&
          priv->width <= allocation.width)
        {
          rows = blxo_icon_view_layout_cols (icon_view, item_height, &x, &maximum_height, priv->rows);
        }

      priv->height = maximum_height;
      priv->width = x;
      priv->rows = rows;
    }

  blxo_icon_view_set_adjustment_upper (priv->hadjustment, priv->width);
  blxo_icon_view_set_adjustment_upper (priv->vadjustment, priv->height);

  if (priv->width != requisition.width
      || priv->height != requisition.height)
    gtk_widget_queue_resize_no_redraw (GTK_WIDGET (icon_view));

  if (gtk_widget_get_realized (GTK_WIDGET (icon_view)))
    {
      gdk_window_resize (priv->bin_window,
                         MAX (priv->width, allocation.width),
                         MAX (priv->height, allocation.height));
    }

  /* drop any pending layout idle source */
  if (priv->layout_idle_id != 0)
    g_source_remove (priv->layout_idle_id);

  gtk_widget_queue_draw (GTK_WIDGET (icon_view));
}



static void
blxo_icon_view_get_cell_area (BlxoIconView         *icon_view,
                             BlxoIconViewItem     *item,
                             BlxoIconViewCellInfo *info,
                             GdkRectangle        *cell_area)
{
  if (icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      cell_area->x = item->box[info->position].x - item->before[info->position];
      cell_area->y = item->area.y;
      cell_area->width = item->box[info->position].width + item->before[info->position] + item->after[info->position];
      cell_area->height = item->area.height;
    }
  else
    {
      cell_area->x = item->area.x;
      cell_area->y = item->box[info->position].y - item->before[info->position];
      cell_area->width = item->area.width;
      cell_area->height = item->box[info->position].height + item->before[info->position] + item->after[info->position];
    }
}



static void
blxo_icon_view_calculate_item_size (BlxoIconView     *icon_view,
                                   BlxoIconViewItem *item)
{
  BlxoIconViewCellInfo *info;
  GList               *lp;
  gchar               *buffer;

  if (G_LIKELY (item->area.width != -1))
    return;

  if (G_UNLIKELY (item->n_cells != icon_view->priv->n_cells))
    {
      /* apply the new cell size */
      item->n_cells = icon_view->priv->n_cells;

      /* release the memory chunk (if any) */
      g_free (item->box);

      /* allocate a single memory chunk for box, after and before */
      buffer = g_malloc0 (item->n_cells * (sizeof (GdkRectangle) + 2 * sizeof (gint)));

      /* assign the memory */
      item->box = (GdkRectangle *) buffer;
      item->after = (gint *) (buffer + item->n_cells * sizeof (GdkRectangle));
      item->before = item->after + item->n_cells;
    }

  blxo_icon_view_set_cell_data (icon_view, item);

  item->area.width = 0;
  item->area.height = 0;
  for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
    {
      info = BLXO_ICON_VIEW_CELL_INFO (lp->data);
      if (G_UNLIKELY (!gtk_cell_renderer_get_visible (info->cell)))
        continue;

#if GTK_CHECK_VERSION (3, 0, 0)
      {
        GtkRequisition req;

        gtk_cell_renderer_get_preferred_size (info->cell, GTK_WIDGET (icon_view),
                                              &req, NULL);

        item->box[info->position].width = req.width;
        item->box[info->position].height = req.height;
      }
#else
      gtk_cell_renderer_get_size (info->cell, GTK_WIDGET (icon_view),
                                  NULL, NULL, NULL,
                                  &item->box[info->position].width,
                                  &item->box[info->position].height);
#endif

      if (icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          item->area.width += item->box[info->position].width + (info->position > 0 ? icon_view->priv->spacing : 0);
          item->area.height = MAX (item->area.height, item->box[info->position].height);
        }
      else
        {
          item->area.width = MAX (item->area.width, item->box[info->position].width);
          item->area.height += item->box[info->position].height + (info->position > 0 ? icon_view->priv->spacing : 0);
        }
    }
}



static void
blxo_icon_view_calculate_item_size2 (BlxoIconView     *icon_view,
                                    BlxoIconViewItem *item,
                                    gint            *max_width,
                                    gint            *max_height)
{
  BlxoIconViewCellInfo *info;
  GdkRectangle        *box;
  GdkRectangle         cell_area;
  gboolean             rtl;
  GList               *lp;
  gint                 spacing;
  gint                 i, k;
  gfloat               cell_xalign, cell_yalign;
  gint                 cell_xpad, cell_ypad;

  rtl = (gtk_widget_get_direction (GTK_WIDGET (icon_view)) == GTK_TEXT_DIR_RTL);

  spacing = icon_view->priv->spacing;

  if (G_LIKELY (icon_view->priv->layout_mode == BLXO_ICON_VIEW_LAYOUT_ROWS))
    {
      item->area.height = 0;
      for (i = 0; i < icon_view->priv->n_cells; ++i)
        {
          if (icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            item->area.height = MAX (item->area.height, max_height[i]);
          else
            item->area.height += max_height[i] + (i > 0 ? spacing : 0);
        }
    }
  else
    {
      item->area.width = 0;
      for (i = 0; i < icon_view->priv->n_cells; ++i)
        {
          if (icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            item->area.width += max_width[i] + (i > 0 ? spacing : 0);
          else
            item->area.width = MAX (item->area.width, max_width[i]);
        }
    }

  cell_area.x = item->area.x;
  cell_area.y = item->area.y;

  for (k = 0; k < 2; ++k)
    {
      for (lp = icon_view->priv->cell_list, i = 0; lp != NULL; lp = lp->next, ++i)
        {
          info = BLXO_ICON_VIEW_CELL_INFO (lp->data);
          if (G_UNLIKELY (!gtk_cell_renderer_get_visible (info->cell) || info->pack == (k ? GTK_PACK_START : GTK_PACK_END)))
            continue;

          gtk_cell_renderer_get_alignment (info->cell, &cell_xalign, &cell_yalign);
          gtk_cell_renderer_get_padding (info->cell, &cell_xpad, &cell_ypad);

          if (icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              cell_area.width = item->box[info->position].width;
              cell_area.height = item->area.height;
            }
          else
            {
              cell_area.width = item->area.width;
              cell_area.height = max_height[i];
            }

          box = item->box + info->position;
          box->x = cell_area.x + (rtl ? (1.0 - cell_xalign) : cell_xalign) * (cell_area.width - box->width - (2 * cell_xpad));
          box->x = MAX (box->x, 0);
          box->y = cell_area.y + cell_yalign * (cell_area.height - box->height - (2 * cell_ypad));
          box->y = MAX (box->y, 0);

          if (icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              item->before[info->position] = item->box[info->position].x - cell_area.x;
              item->after[info->position] = cell_area.width - item->box[info->position].width - item->before[info->position];
              cell_area.x += cell_area.width + spacing;
            }
          else
            {
              if (item->box[info->position].width > item->area.width)
                {
                  item->area.width = item->box[info->position].width;
                  cell_area.width = item->area.width;
                }
              item->before[info->position] = item->box[info->position].y - cell_area.y;
              item->after[info->position] = cell_area.height - item->box[info->position].height - item->before[info->position];
              cell_area.y += cell_area.height + spacing;
            }
        }
    }

  if (G_UNLIKELY (rtl && icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL))
    {
      for (i = 0; i < icon_view->priv->n_cells; i++)
        item->box[i].x = item->area.x + item->area.width - (item->box[i].x + item->box[i].width - item->area.x);
    }
}



static void
blxo_icon_view_invalidate_sizes (BlxoIconView *icon_view)
{
  GList *lp;

  for (lp = icon_view->priv->items; lp != NULL; lp = lp->next)
    BLXO_ICON_VIEW_ITEM (lp->data)->area.width = -1;
  blxo_icon_view_queue_layout (icon_view);
}


#if GTK_CHECK_VERSION (3, 0, 0)
static void
blxo_icon_view_paint_item (BlxoIconView     *icon_view,
                          BlxoIconViewItem *item,
                          cairo_t         *cr,
                          gint             x,
                          gint             y,
                          gboolean         draw_focus)
{
  GtkCellRendererState flags = 0;
  BlxoIconViewCellInfo *info;
  GtkStateType         state;
  GdkRectangle         cell_area;
  GdkRectangle         aligned_area;
  GtkStyleContext     *style_context;
  GList               *lp;

  if (G_UNLIKELY (icon_view->priv->model == NULL))
    return;

  blxo_icon_view_set_cell_data (icon_view, item);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (icon_view));
  state = gtk_widget_get_state_flags (GTK_WIDGET (icon_view));

  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_CELL);

  state &= ~(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_PRELIGHT);

  if (G_UNLIKELY (BLXO_ICON_VIEW_FLAG_SET (icon_view, BLXO_ICON_VIEW_DRAW_KEYFOCUS)
    && (state & GTK_STATE_FLAG_FOCUSED) && item == icon_view->priv->cursor_item))
    {
      flags |= GTK_CELL_RENDERER_FOCUSED;
    }

  if (G_UNLIKELY (item->selected))
    {
      state |= GTK_STATE_FLAG_SELECTED;
      flags |= GTK_CELL_RENDERER_SELECTED;
    }

  if (G_UNLIKELY (icon_view->priv->prelit_item == item))
    {
      state |= GTK_STATE_FLAG_PRELIGHT;
      flags |= GTK_CELL_RENDERER_PRELIT;
    }

  gtk_style_context_set_state (style_context, state);

  for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
    {
      info = BLXO_ICON_VIEW_CELL_INFO (lp->data);

      cairo_save (cr);

      if (G_UNLIKELY (!gtk_cell_renderer_get_visible (info->cell)))
        continue;

      blxo_icon_view_get_cell_area (icon_view, item, info, &cell_area);

      cell_area.x = x - item->area.x + cell_area.x;
      cell_area.y = y - item->area.y + cell_area.y;

      /* FIXME: this is bad CSS usage */
      if (info->is_text)
        {
          gtk_cell_renderer_get_aligned_area (info->cell,
                                              GTK_WIDGET (icon_view),
                                              flags,
                                              &cell_area,
                                              &aligned_area);

          gtk_render_background (style_context, cr,
                                 aligned_area.x, aligned_area.y,
                                 aligned_area.width, aligned_area.height);

          gtk_render_frame (style_context, cr,
                            aligned_area.x, aligned_area.y,
                            aligned_area.width, aligned_area.height);
        }

      gtk_cell_renderer_render (info->cell,
                                cr,
                                GTK_WIDGET (icon_view),
                                &cell_area, &cell_area, flags);

      cairo_restore (cr);
    }

  gtk_style_context_restore (style_context);
}

#else
static void
blxo_icon_view_paint_item (BlxoIconView     *icon_view,
                          BlxoIconViewItem *item,
                          GdkRectangle    *area,
                          GdkDrawable     *drawable,
                          gint             x,
                          gint             y,
                          gboolean         draw_focus)
{
  GtkCellRendererState flags;
  BlxoIconViewCellInfo *info;
  GtkStateType         state;
  GdkRectangle         cell_area;
  cairo_t             *cr;
  GList               *lp;
  gint                 x_0;
  gint                 y_0;
  gint                 x_1;
  gint                 y_1;

  if (G_UNLIKELY (icon_view->priv->model == NULL))
    return;

  blxo_icon_view_set_cell_data (icon_view, item);

  if (item->selected)
    {
      flags = GTK_CELL_RENDERER_SELECTED;
      state = gtk_widget_has_focus (GTK_WIDGET (icon_view)) ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE;

      /* FIXME We hardwire background drawing behind text cell renderers
       * here. This is ugly, but it's done to be consistent with GtkIconView.
       * The additional info->is_text attribute is used for performance
       * optimization and should be removed alongside the following code. */

      cr = gdk_cairo_create (drawable);

      for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
        {
          info = BLXO_ICON_VIEW_CELL_INFO (lp->data);

          if (G_UNLIKELY (!gtk_cell_renderer_get_visible (info->cell)))
            continue;

          if (info->is_text)
            {
              blxo_icon_view_get_cell_area (icon_view, item, info, &cell_area);

              x_0 = x - item->area.x + cell_area.x;
              y_0 = x - item->area.x + cell_area.y;
              x_1 = x_0 + cell_area.width;
              y_1 = y_0 + cell_area.height;

              cairo_move_to (cr, x_0 + 5, y_0);
              cairo_line_to (cr, x_1 - 5, y_0);
              cairo_curve_to (cr, x_1 - 5, y_0, x_1, y_0, x_1, y_0 + 5);
              cairo_line_to (cr, x_1, y_1 - 5);
              cairo_curve_to (cr, x_1, y_1 - 5, x_1, y_1, x_1 - 5, y_1);
              cairo_line_to (cr, x_0 + 5, y_1);
              cairo_curve_to (cr, x_0 + 5, y_1, x_0, y_1, x_0, y_1 - 5);
              cairo_line_to (cr, x_0, y_0 + 5);
              cairo_curve_to (cr, x_0, y_0 + 5, x_0, y_0, x_0 + 5, y_0);

              gdk_cairo_set_source_color (cr, &gtk_widget_get_style (GTK_WIDGET (icon_view))->base[state]);

              cairo_fill (cr);
            }
        }

      cairo_destroy (cr);

      /* FIXME Ugly code ends here */
    }
  else
    {
      flags = 0;
      state = GTK_STATE_NORMAL;
    }

  if (G_UNLIKELY (icon_view->priv->prelit_item == item))
    flags |= GTK_CELL_RENDERER_PRELIT;
  if (G_UNLIKELY (BLXO_ICON_VIEW_FLAG_SET (icon_view, BLXO_ICON_VIEW_DRAW_KEYFOCUS) && icon_view->priv->cursor_item == item))
    flags |= GTK_CELL_RENDERER_FOCUSED;

#ifdef DEBUG_ICON_VIEW
  gdk_draw_rectangle (drawable,
                      GTK_WIDGET (icon_view)->style->black_gc,
                      FALSE,
                      x, y,
                      item->area.width, item->area.height);
#endif

  for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
    {
      info = BLXO_ICON_VIEW_CELL_INFO (lp->data);

      if (G_UNLIKELY (!gtk_cell_renderer_get_visible (info->cell)))
        continue;

      blxo_icon_view_get_cell_area (icon_view, item, info, &cell_area);

#ifdef DEBUG_ICON_VIEW
      gdk_draw_rectangle (drawable,
                          GTK_WIDGET (icon_view)->style->black_gc,
                          FALSE,
                          x - item->area.x + cell_area.x,
                          y - item->area.y + cell_area.y,
                          cell_area.width, cell_area.height);

      gdk_draw_rectangle (drawable,
                          GTK_WIDGET (icon_view)->style->black_gc,
                          FALSE,
                          x - item->area.x + item->box[info->position].x,
                          y - item->area.y + item->box[info->position].y,
                          item->box[info->position].width, item->box[info->position].height);
#endif

      cell_area.x = x - item->area.x + cell_area.x;
      cell_area.y = y - item->area.y + cell_area.y;

      gtk_cell_renderer_render (info->cell,
                                drawable,
                                GTK_WIDGET (icon_view),
                                &cell_area, &cell_area, area, flags);
    }
}
#endif


static void
blxo_icon_view_queue_draw_item (BlxoIconView     *icon_view,
                               BlxoIconViewItem *item)
{
  GdkRectangle rect;
  gint         focus_width;

  gtk_widget_style_get (GTK_WIDGET (icon_view),
                        "focus-line-width", &focus_width,
                        NULL);

  rect.x = item->area.x - focus_width;
  rect.y = item->area.y - focus_width;
  rect.width = item->area.width + 2 * focus_width;
  rect.height = item->area.height + 2 * focus_width;

  if (icon_view->priv->bin_window)
    gdk_window_invalidate_rect (icon_view->priv->bin_window, &rect, TRUE);
}



static gboolean
layout_callback (gpointer user_data)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (user_data);

  blxo_icon_view_layout (icon_view);

  return FALSE;
}



static void
layout_destroy (gpointer user_data)
{
  BLXO_ICON_VIEW (user_data)->priv->layout_idle_id = 0;
}



static void
blxo_icon_view_queue_layout (BlxoIconView *icon_view)
{
  if (G_UNLIKELY (icon_view->priv->layout_idle_id == 0))
    icon_view->priv->layout_idle_id = gdk_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE, layout_callback, icon_view, layout_destroy);
}



static void
blxo_icon_view_set_cursor_item (BlxoIconView     *icon_view,
                               BlxoIconViewItem *item,
                               gint             cursor_cell)
{
  if (icon_view->priv->cursor_item == item &&
      (cursor_cell < 0 || cursor_cell == icon_view->priv->cursor_cell))
    return;

  if (icon_view->priv->cursor_item != NULL)
    blxo_icon_view_queue_draw_item (icon_view, icon_view->priv->cursor_item);

  icon_view->priv->cursor_item = item;
  if (cursor_cell >= 0)
    icon_view->priv->cursor_cell = cursor_cell;

  blxo_icon_view_queue_draw_item (icon_view, item);
}



static BlxoIconViewItem*
blxo_icon_view_get_item_at_coords (const BlxoIconView    *icon_view,
                                  gint                  x,
                                  gint                  y,
                                  gboolean              only_in_cell,
                                  BlxoIconViewCellInfo **cell_at_pos)
{
  const BlxoIconViewPrivate *priv = icon_view->priv;
  BlxoIconViewCellInfo      *info;
  BlxoIconViewItem          *item;
  GdkRectangle              box;
  const GList              *items;
  const GList              *lp;

  for (items = priv->items; items != NULL; items = items->next)
    {
      item = items->data;
      if (x >= item->area.x - priv->row_spacing / 2 && x <= item->area.x + item->area.width + priv->row_spacing / 2 &&
          y >= item->area.y - priv->column_spacing / 2 && y <= item->area.y + item->area.height + priv->column_spacing / 2)
        {
          if (only_in_cell || cell_at_pos)
            {
              blxo_icon_view_set_cell_data (icon_view, item);
              for (lp = priv->cell_list; lp != NULL; lp = lp->next)
                {
                  /* check if the cell is visible */
                  info = (BlxoIconViewCellInfo *) lp->data;
                  if (!gtk_cell_renderer_get_visible (info->cell))
                    continue;

                  box = item->box[info->position];
                  if ((x >= box.x && x <= box.x + box.width &&
                       y >= box.y && y <= box.y + box.height))
                    {
                      if (cell_at_pos != NULL)
                        *cell_at_pos = info;

                      return item;
                    }
                }

              if (only_in_cell)
                return NULL;

              if (cell_at_pos != NULL)
                *cell_at_pos = NULL;
            }

          return item;
        }
    }

  return NULL;
}



static void
blxo_icon_view_select_item (BlxoIconView      *icon_view,
                           BlxoIconViewItem  *item)
{
  if (item->selected || icon_view->priv->selection_mode == GTK_SELECTION_NONE)
    return;
  else if (icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    blxo_icon_view_unselect_all_internal (icon_view);

  item->selected = TRUE;

  blxo_icon_view_queue_draw_item (icon_view, item);

  g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}



static void
blxo_icon_view_unselect_item (BlxoIconView      *icon_view,
                             BlxoIconViewItem  *item)
{
  if (!item->selected)
    return;

  if (icon_view->priv->selection_mode == GTK_SELECTION_NONE ||
      icon_view->priv->selection_mode == GTK_SELECTION_BROWSE)
    return;

  item->selected = FALSE;

  g_signal_emit (G_OBJECT (icon_view), icon_view_signals[SELECTION_CHANGED], 0);

  blxo_icon_view_queue_draw_item (icon_view, item);
}



static void
blxo_icon_view_row_changed (GtkTreeModel *model,
                           GtkTreePath  *path,
                           GtkTreeIter  *iter,
                           BlxoIconView  *icon_view)
{
  BlxoIconViewItem *item;

  item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);

  /* stop editing this item */
  if (G_UNLIKELY (item == icon_view->priv->edited_item))
    blxo_icon_view_stop_editing (icon_view, TRUE);

  /* emit "selection-changed" if the item is selected */
  if (G_UNLIKELY (item->selected))
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);

  /* recalculate layout (a value of -1 for width
   * indicates that the item needs to be layouted).
   */
  item->area.width = -1;
  blxo_icon_view_queue_layout (icon_view);
}



static void
blxo_icon_view_row_inserted (GtkTreeModel *model,
                            GtkTreePath  *path,
                            GtkTreeIter  *iter,
                            BlxoIconView  *icon_view)
{
  BlxoIconViewItem *item;
  gint             idx;

  idx = gtk_tree_path_get_indices (path)[0];

  /* allocate the new item */
  item = g_slice_new0 (BlxoIconViewItem);
  item->iter = *iter;
  item->area.width = -1;
  icon_view->priv->items = g_list_insert (icon_view->priv->items, item, idx);

  /* recalculate the layout */
  blxo_icon_view_queue_layout (icon_view);
}



static void
blxo_icon_view_row_deleted (GtkTreeModel *model,
                           GtkTreePath  *path,
                           BlxoIconView  *icon_view)
{
  BlxoIconViewItem *item;
  gboolean         changed = FALSE;
  GList           *list;

  /* determine the position and the item for the path */
  list = g_list_nth (icon_view->priv->items, gtk_tree_path_get_indices (path)[0]);
  item = list->data;

  if (G_UNLIKELY (item == icon_view->priv->edited_item))
    blxo_icon_view_stop_editing (icon_view, TRUE);

  /* use the next item (if any) as anchor, else use prev, otherwise reset anchor */
  if (G_UNLIKELY (item == icon_view->priv->anchor_item))
    icon_view->priv->anchor_item = (list->next != NULL) ? list->next->data : ((list->prev != NULL) ? list->prev->data : NULL);

  /* use the next item (if any) as cursor, else use prev, otherwise reset cursor */
  if (G_UNLIKELY (item == icon_view->priv->cursor_item))
    icon_view->priv->cursor_item = (list->next != NULL) ? list->next->data : ((list->prev != NULL) ? list->prev->data : NULL);

  if (G_UNLIKELY (item == icon_view->priv->prelit_item))
    {
      /* reset the prelit item */
      icon_view->priv->prelit_item = NULL;

      /* cancel any pending single click timer */
      if (G_UNLIKELY (icon_view->priv->single_click_timeout_id != 0))
        g_source_remove (icon_view->priv->single_click_timeout_id);

      /* in single click mode, we also reset the cursor when realized */
      if (G_UNLIKELY (icon_view->priv->single_click && gtk_widget_get_realized (GTK_WIDGET (icon_view))))
        gdk_window_set_cursor (icon_view->priv->bin_window, NULL);
    }

  /* check if the selection changed */
  if (G_UNLIKELY (item->selected))
    changed = TRUE;

  /* release the item resources */
  g_free (item->box);

  /* drop the item from the list */
  icon_view->priv->items = g_list_delete_link (icon_view->priv->items, list);

  /* release the item */
  g_slice_free (BlxoIconViewItem, item);

  /* recalculate the layout */
  blxo_icon_view_queue_layout (icon_view);

  /* if we removed a previous selected item, we need
   * to tell others that we have a new selection.
   */
  if (G_UNLIKELY (changed))
    g_signal_emit (G_OBJECT (icon_view), icon_view_signals[SELECTION_CHANGED], 0);
}



static void
blxo_icon_view_rows_reordered (GtkTreeModel *model,
                              GtkTreePath  *parent,
                              GtkTreeIter  *iter,
                              gint         *new_order,
                              BlxoIconView  *icon_view)
{
  GList          **list_array;
  GList           *list;
  gint            *order;
  gint              length;
  gint              i;

  /* cancel any editing attempt */
  blxo_icon_view_stop_editing (icon_view, TRUE);

  /* determine the number of items to reorder */
  length = gtk_tree_model_iter_n_children (model, NULL);
  if (G_UNLIKELY (length == 0))
    return;

  list_array = g_newa (GList *, length);
  order = g_newa (gint, length);

  for (i = 0; i < length; i++)
    order[new_order[i]] = i;

  for (i = 0, list = icon_view->priv->items; list != NULL; list = list->next, i++)
    list_array[order[i]] = list;

  /* hook up the first item */
  icon_view->priv->items = list_array[0];
  list_array[0]->prev = NULL;

  /* hook up the remaining items */
  for (i = 1; i < length; ++i)
    {
      list_array[i - 1]->next = list_array[i];
      list_array[i]->prev = list_array[i - 1];
    }

  /* hook up the last item */
  list_array[length - 1]->next = NULL;

  blxo_icon_view_queue_layout (icon_view);
}



static void
blxo_icon_view_add_move_binding (GtkBindingSet  *binding_set,
                                guint           keyval,
                                guint           modmask,
                                GtkMovementStep step,
                                gint            count)
{
  gtk_binding_entry_add_signal (binding_set, keyval, modmask, "move-cursor", 2, G_TYPE_ENUM, step, G_TYPE_INT, count);

  /* skip shift+n and shift+p because this blocks type-ahead search.
   * see https://bugzilla.xfce.org/show_bug.cgi?id=4633
   */
  if (G_LIKELY (keyval != GDK_KEY_p && keyval != GDK_KEY_n))
    gtk_binding_entry_add_signal (binding_set, keyval, GDK_SHIFT_MASK, "move-cursor", 2, G_TYPE_ENUM, step, G_TYPE_INT, count);

  if ((modmask & GDK_CONTROL_MASK) != GDK_CONTROL_MASK)
    {
      gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "move-cursor", 2, G_TYPE_ENUM, step, G_TYPE_INT, count);
      gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK, "move-cursor", 2, G_TYPE_ENUM, step, G_TYPE_INT, count);
    }
}



static gboolean
blxo_icon_view_real_move_cursor (BlxoIconView     *icon_view,
                                GtkMovementStep  step,
                                gint             count)
{
  GdkModifierType state;

  _blxo_return_val_if_fail (BLXO_ICON_VIEW (icon_view), FALSE);
  _blxo_return_val_if_fail (step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
                           step == GTK_MOVEMENT_VISUAL_POSITIONS ||
                           step == GTK_MOVEMENT_DISPLAY_LINES ||
                           step == GTK_MOVEMENT_PAGES ||
                           step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return FALSE;

  blxo_icon_view_stop_editing (icon_view, FALSE);
  BLXO_ICON_VIEW_SET_FLAG (icon_view, BLXO_ICON_VIEW_DRAW_KEYFOCUS);
  gtk_widget_grab_focus (GTK_WIDGET (icon_view));

  if (gtk_get_current_event_state (&state))
    {
      if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
        icon_view->priv->ctrl_pressed = TRUE;
      if ((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
        icon_view->priv->shift_pressed = TRUE;
    }
  /* else we assume not pressed */

  switch (step)
    {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      blxo_icon_view_move_cursor_left_right (icon_view, count);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      blxo_icon_view_move_cursor_up_down (icon_view, count);
      break;
    case GTK_MOVEMENT_PAGES:
      blxo_icon_view_move_cursor_page_up_down (icon_view, count);
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      blxo_icon_view_move_cursor_start_end (icon_view, count);
      break;
    default:
      g_assert_not_reached ();
    }

  icon_view->priv->ctrl_pressed = FALSE;
  icon_view->priv->shift_pressed = FALSE;

  return TRUE;
}



static gint
find_cell (BlxoIconView     *icon_view,
           BlxoIconViewItem *item,
           gint             cell,
           GtkOrientation   orientation,
           gint             step,
           gint            *count)
{
  gint n_focusable;
  gint *focusable;
  gint first_text;
  gint current;
  gint i, k;
  GList *l;
  GtkCellRendererMode mode;

  if (icon_view->priv->orientation != orientation)
    return cell;

  blxo_icon_view_set_cell_data (icon_view, item);

  focusable = g_new0 (gint, icon_view->priv->n_cells);
  n_focusable = 0;

  first_text = 0;
  current = 0;
  for (k = 0; k < 2; k++)
    for (l = icon_view->priv->cell_list, i = 0; l; l = l->next, i++)
      {
        BlxoIconViewCellInfo *info = (BlxoIconViewCellInfo *)l->data;

        if (info->pack == (k ? GTK_PACK_START : GTK_PACK_END))
          continue;

        if (!gtk_cell_renderer_get_visible (info->cell))
          continue;

        if (GTK_IS_CELL_RENDERER_TEXT (info->cell))
          first_text = i;

        g_object_get (info->cell, "mode", &mode, NULL);
        if (mode != GTK_CELL_RENDERER_MODE_INERT)
          {
            if (cell == i)
              current = n_focusable;

            focusable[n_focusable] = i;

            n_focusable++;
          }
      }

  if (n_focusable == 0)
    focusable[n_focusable++] = first_text;

  if (cell < 0)
    {
      current = step > 0 ? 0 : n_focusable - 1;
    }

  if (current + *count < 0)
    {
      cell = -1;
      *count = current + *count;
    }
  else if (current + *count > n_focusable - 1)
    {
      cell = -1;
      *count = current + *count - (n_focusable - 1);
    }
  else
    {
      cell = focusable[current + *count];
      *count = 0;
    }

  g_free (focusable);

  return cell;
}



static BlxoIconViewItem *
find_item_page_up_down (BlxoIconView     *icon_view,
                        BlxoIconViewItem *current,
                        gint             count)
{
  GList *item = g_list_find (icon_view->priv->items, current);
  GList *next;
  gint   col = current->col;
  gint   y = current->area.y + count * gtk_adjustment_get_page_size (icon_view->priv->vadjustment);

  if (count > 0)
    {
      for (; item != NULL; item = item->next)
        {
          for (next = item->next; next; next = next->next)
            if (BLXO_ICON_VIEW_ITEM (next->data)->col == col)
              break;

          if (next == NULL || BLXO_ICON_VIEW_ITEM (next->data)->area.y > y)
            break;
        }
    }
  else
    {
      for (; item != NULL; item = item->prev)
        {
          for (next = item->prev; next; next = next->prev)
            if (BLXO_ICON_VIEW_ITEM (next->data)->col == col)
              break;

          if (next == NULL || BLXO_ICON_VIEW_ITEM (next->data)->area.y < y)
            break;
        }
    }

  return (item != NULL) ? item->data : NULL;
}



static gboolean
blxo_icon_view_select_all_between (BlxoIconView     *icon_view,
                                  BlxoIconViewItem *anchor,
                                  BlxoIconViewItem *cursor)
{
  GList *items;
  BlxoIconViewItem *item, *last;
  gboolean dirty = FALSE;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      item = items->data;

      if (item == anchor)
        {
          last = cursor;
          break;
        }
      else if (item == cursor)
        {
          last = anchor;
          break;
        }
    }

  for (; items; items = items->next)
    {
      item = items->data;

      if (!item->selected)
        dirty = TRUE;

      item->selected = TRUE;

      blxo_icon_view_queue_draw_item (icon_view, item);

      if (item == last)
        break;
    }

  return dirty;
}



static void
blxo_icon_view_move_cursor_up_down (BlxoIconView *icon_view,
                                   gint         count)
{
  BlxoIconViewItem  *item;
  gboolean          dirty = FALSE;
  GList            *list;
  gint              cell = -1;
  gint              step;
  GtkDirectionType  direction;
  GtkWidget        *toplevel;

  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return;

  direction = count < 0 ? GTK_DIR_UP : GTK_DIR_DOWN;

  if (!icon_view->priv->cursor_item)
    {
      if (count > 0)
        list = icon_view->priv->items;
      else
        list = g_list_last (icon_view->priv->items);

      item = list ? list->data : NULL;
    }
  else
    {
      item = icon_view->priv->cursor_item;
      cell = icon_view->priv->cursor_cell;
      step = count > 0 ? 1 : -1;
      while (item)
        {
          cell = find_cell (icon_view, item, cell,
                            GTK_ORIENTATION_VERTICAL,
                            step, &count);
          if (count == 0)
            break;

          /* determine the list position for the item */
          list = g_list_find (icon_view->priv->items, item);

          if (G_LIKELY (icon_view->priv->layout_mode == BLXO_ICON_VIEW_LAYOUT_ROWS))
            {
              /* determine the item in the next/prev row */
              if (step > 0)
                {
                  for (list = list->next; list != NULL; list = list->next)
                    if (BLXO_ICON_VIEW_ITEM (list->data)->row == item->row + step
                        && BLXO_ICON_VIEW_ITEM (list->data)->col == item->col)
                      break;
                 }
              else
                {
                  for (list = list->prev; list != NULL; list = list->prev)
                    if (BLXO_ICON_VIEW_ITEM (list->data)->row == item->row + step
                        && BLXO_ICON_VIEW_ITEM (list->data)->col == item->col)
                      break;
                }
            }
          else
            {
              list = (step > 0) ? list->next : list->prev;
            }

          /* check if we found a matching item */
          item = (list != NULL) ? list->data : NULL;

          count = count - step;
        }
    }

  if (!item)
    {
      if (!gtk_widget_keynav_failed (GTK_WIDGET (icon_view), direction))
        {
          toplevel = gtk_widget_get_toplevel (GTK_WIDGET (icon_view));
          if (toplevel != NULL)
            {
              gtk_widget_child_focus (toplevel,
                                      direction == GTK_DIR_UP ?
                                          GTK_DIR_TAB_BACKWARD :
                                          GTK_DIR_TAB_FORWARD);
            }
        }

      return;
    }

  if (icon_view->priv->ctrl_pressed ||
      !icon_view->priv->shift_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  blxo_icon_view_set_cursor_item (icon_view, item, cell);

  if (!icon_view->priv->ctrl_pressed &&
      icon_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = blxo_icon_view_unselect_all_internal (icon_view);
      dirty = blxo_icon_view_select_all_between (icon_view,
                                                icon_view->priv->anchor_item,
                                                item) || dirty;
    }

  blxo_icon_view_scroll_to_item (icon_view, item);

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}



static void
blxo_icon_view_move_cursor_page_up_down (BlxoIconView *icon_view,
                                        gint         count)
{
  BlxoIconViewItem *item;
  gboolean dirty = FALSE;

  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return;

  if (!icon_view->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
        list = icon_view->priv->items;
      else
        list = g_list_last (icon_view->priv->items);

      item = list ? list->data : NULL;
    }
  else
    item = find_item_page_up_down (icon_view,
                                   icon_view->priv->cursor_item,
                                   count);

  if (!item)
    return;

  if (icon_view->priv->ctrl_pressed ||
      !icon_view->priv->shift_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  blxo_icon_view_set_cursor_item (icon_view, item, -1);

  if (!icon_view->priv->ctrl_pressed &&
      icon_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = blxo_icon_view_unselect_all_internal (icon_view);
      dirty = blxo_icon_view_select_all_between (icon_view,
                                                icon_view->priv->anchor_item,
                                                item) || dirty;
    }

  blxo_icon_view_scroll_to_item (icon_view, item);

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}



static void
blxo_icon_view_move_cursor_left_right (BlxoIconView *icon_view,
                                      gint         count)
{
  BlxoIconViewItem  *item;
  gboolean          dirty = FALSE;
  GList            *list;
  gint              cell = -1;
  gint              step;
  GtkDirectionType  direction;
  GtkWidget        *toplevel;

  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return;

  if (gtk_widget_get_direction (GTK_WIDGET (icon_view)) == GTK_TEXT_DIR_RTL)
    count *= -1;

  direction = count < 0 ? GTK_DIR_LEFT : GTK_DIR_RIGHT;

  if (!icon_view->priv->cursor_item)
    {
      if (count > 0)
        list = icon_view->priv->items;
      else
        list = g_list_last (icon_view->priv->items);

      item = list ? list->data : NULL;
    }
  else
    {
      item = icon_view->priv->cursor_item;
      cell = icon_view->priv->cursor_cell;
      step = count > 0 ? 1 : -1;
      while (item)
        {
          cell = find_cell (icon_view, item, cell,
                            GTK_ORIENTATION_HORIZONTAL,
                            step, &count);
          if (count == 0)
            break;

          /* lookup the item in the list */
          list = g_list_find (icon_view->priv->items, item);

          if (G_LIKELY (icon_view->priv->layout_mode == BLXO_ICON_VIEW_LAYOUT_ROWS))
            {
              /* determine the next/prev list item depending on step,
               * support wrapping around on the edges, as requested
               * in https://bugzilla.xfce.org/show_bug.cgi?id=1623.
               */
              list = (step > 0) ? list->next : list->prev;
            }
          else
            {
              /* determine the item in the next/prev row */
              if (step > 0)
                {
                  for (list = list->next; list != NULL; list = list->next)
                    if (BLXO_ICON_VIEW_ITEM (list->data)->col == item->col + step
                        && BLXO_ICON_VIEW_ITEM (list->data)->row == item->row)
                      break;
                 }
              else
                {
                  for (list = list->prev; list != NULL; list = list->prev)
                    if (BLXO_ICON_VIEW_ITEM (list->data)->col == item->col + step
                        && BLXO_ICON_VIEW_ITEM (list->data)->row == item->row)
                      break;
                }
            }

          /* determine the item for the list position (if any) */
          item = (list != NULL) ? list->data : NULL;

          count = count - step;
        }
    }

  if (!item)
    {
      if (!gtk_widget_keynav_failed (GTK_WIDGET (icon_view), direction))
        {
          toplevel = gtk_widget_get_toplevel (GTK_WIDGET (icon_view));
          if (toplevel != NULL)
            {
              gtk_widget_child_focus (toplevel,
                                      direction == GTK_DIR_LEFT ?
                                          GTK_DIR_TAB_BACKWARD :
                                          GTK_DIR_TAB_FORWARD);
            }
        }

      return;
    }

  if (icon_view->priv->ctrl_pressed ||
      !icon_view->priv->shift_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  blxo_icon_view_set_cursor_item (icon_view, item, cell);

  if (!icon_view->priv->ctrl_pressed &&
      icon_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = blxo_icon_view_unselect_all_internal (icon_view);
      dirty = blxo_icon_view_select_all_between (icon_view,
                                                icon_view->priv->anchor_item,
                                                item) || dirty;
    }

  blxo_icon_view_scroll_to_item (icon_view, item);

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}



static void
blxo_icon_view_move_cursor_start_end (BlxoIconView *icon_view,
                                     gint         count)
{
  BlxoIconViewItem *item;
  gboolean         dirty = FALSE;
  GList           *lp;

  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return;

  lp = (count < 0) ? icon_view->priv->items : g_list_last (icon_view->priv->items);
  if (G_UNLIKELY (lp == NULL))
    return;

  item = BLXO_ICON_VIEW_ITEM (lp->data);
  if (icon_view->priv->ctrl_pressed ||
      !icon_view->priv->shift_pressed ||
      !icon_view->priv->anchor_item ||
      icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    icon_view->priv->anchor_item = item;

  blxo_icon_view_set_cursor_item (icon_view, item, -1);

  if (!icon_view->priv->ctrl_pressed &&
      icon_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = blxo_icon_view_unselect_all_internal (icon_view);
      dirty = blxo_icon_view_select_all_between (icon_view,
                                                icon_view->priv->anchor_item,
                                                item) || dirty;
    }

  blxo_icon_view_scroll_to_item (icon_view, item);

  if (G_UNLIKELY (dirty))
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}



/* Get the actual size needed by an item (as opposed to the size
 * allocated based on the largest item in the same row/column).
 */
static void
blxo_icon_view_get_item_needed_size (BlxoIconView     *icon_view,
                                    BlxoIconViewItem *item,
                                    gint            *width,
                                    gint            *height)
{
  GList               *lp;
  BlxoIconViewCellInfo *info;

  *width = 0;
  *height = 0;

  for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
    {
      info = BLXO_ICON_VIEW_CELL_INFO (lp->data);
      if (G_UNLIKELY (!gtk_cell_renderer_get_visible (info->cell)))
        continue;

      if (icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *width += item->box[info->position].width
                  + (info->position > 0 ? icon_view->priv->spacing : 0);
          *height = MAX (*height, item->box[info->position].height);
        }
      else
        {
          *width = MAX (*width, item->box[info->position].width);
          *height += item->box[info->position].height
                   + (info->position > 0 ? icon_view->priv->spacing : 0);
        }
    }
}



static void
blxo_icon_view_scroll_to_item (BlxoIconView     *icon_view,
                              BlxoIconViewItem *item)
{
  gint x, y;
  gint focus_width;
  gint item_width, item_height;
  GtkAllocation allocation;
  GtkTreePath *path;

  /* Delay scrolling if either not realized or pending layout() */
  if (!gtk_widget_get_realized (GTK_WIDGET(icon_view)) || icon_view->priv->layout_idle_id != 0)
    {
      /* release the previous scroll_to_path reference */
      if (G_UNLIKELY (icon_view->priv->scroll_to_path != NULL))
        gtk_tree_row_reference_free (icon_view->priv->scroll_to_path);

      /* remember a reference for the new path and settings */

      path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1);
      icon_view->priv->scroll_to_path = gtk_tree_row_reference_new_proxy (G_OBJECT (icon_view), icon_view->priv->model, path);
      gtk_tree_path_free (path);

      icon_view->priv->scroll_to_use_align = FALSE;

      return;
    }

  gtk_widget_style_get (GTK_WIDGET (icon_view),
                        "focus-line-width", &focus_width,
                        NULL);

  gdk_window_get_position (icon_view->priv->bin_window, &x, &y);
  blxo_icon_view_get_item_needed_size (icon_view, item, &item_width, &item_height);
  gtk_widget_get_allocation (GTK_WIDGET (icon_view), &allocation);

  /*
   * If an item reaches beyond the edges of the view, we scroll just enough
   * to make as much of it visible as possible.  This avoids interfering
   * with double-click (since the second click will not scroll differently),
   * prevents narrow items in wide columns from being scrolled out of view
   * when selected, and ensures that items will be brought into view when
   * selected even if it was done by a keystroke instead of a mouse click.
   * See bugs 1683 and 6014 for some problems seen in the past.
   */

  if (y + item->area.y - focus_width < 0)
    {
      gtk_adjustment_set_value (icon_view->priv->vadjustment,
                                gtk_adjustment_get_value (icon_view->priv->vadjustment) + y + item->area.y - focus_width);
    }
  else if (y + item->area.y + item_height + focus_width > allocation.height
        && y + item->area.y - focus_width > 0)
    {
      gtk_adjustment_set_value (icon_view->priv->vadjustment,
                                gtk_adjustment_get_value (icon_view->priv->vadjustment)
                                + MIN (y + item->area.y - focus_width,
                                       y + item->area.y + item_height + focus_width - allocation.height));
    }

  if (x + item->area.x - focus_width < 0)
    {
      gtk_adjustment_set_value (icon_view->priv->hadjustment,
                                gtk_adjustment_get_value (icon_view->priv->hadjustment) + x + item->area.x - focus_width);
    }
  else if (x + item->area.x + item_width + focus_width > allocation.width
        && x + item->area.x - focus_width > 0)
    {
      gtk_adjustment_set_value (icon_view->priv->hadjustment,
                                gtk_adjustment_get_value (icon_view->priv->hadjustment)
                                + MIN (x + item->area.x - focus_width,
                                       x + item->area.x + item_width + focus_width - allocation.width));
    }

  /* Prior to GTK 3.18, we need to emit "changed" ourselves */
#if !GTK_CHECK_VERSION (3, 18, 0)
  gtk_adjustment_changed (icon_view->priv->hadjustment);
  gtk_adjustment_changed (icon_view->priv->vadjustment);
#endif
}



static BlxoIconViewCellInfo *
blxo_icon_view_get_cell_info (BlxoIconView     *icon_view,
                             GtkCellRenderer *renderer)
{
  GList *lp;

  for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
    if (BLXO_ICON_VIEW_CELL_INFO (lp->data)->cell == renderer)
      return lp->data;

  return NULL;
}



static void
blxo_icon_view_set_cell_data (const BlxoIconView *icon_view,
                             BlxoIconViewItem   *item)
{
  BlxoIconViewCellInfo *info;
  GtkTreePath         *path;
  GtkTreeIter          iter;
  GValue               value = {0, };
  GSList              *slp;
  GList               *lp;

  if (G_UNLIKELY (!BLXO_ICON_VIEW_FLAG_SET (icon_view, BLXO_ICON_VIEW_ITERS_PERSIST)))
    {
      path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1);
      gtk_tree_model_get_iter (icon_view->priv->model, &iter, path);
      gtk_tree_path_free (path);
    }
  else
    {
      iter = item->iter;
    }

  for (lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next)
    {
      info = BLXO_ICON_VIEW_CELL_INFO (lp->data);

      for (slp = info->attributes; slp != NULL && slp->next != NULL; slp = slp->next->next)
        {
          gtk_tree_model_get_value (icon_view->priv->model, &iter, GPOINTER_TO_INT (slp->next->data), &value);
          g_object_set_property (G_OBJECT (info->cell), slp->data, &value);
          g_value_unset (&value);
        }

      if (G_UNLIKELY (info->func != NULL))
        (*info->func) (GTK_CELL_LAYOUT (icon_view), info->cell, icon_view->priv->model, &iter, info->func_data);
    }
}



static void
free_cell_attributes (BlxoIconViewCellInfo *info)
{
  GSList *lp;

  for (lp = info->attributes; lp != NULL && lp->next != NULL; lp = lp->next->next)
    g_free (lp->data);
  g_slist_free (info->attributes);
  info->attributes = NULL;
}



static void
free_cell_info (BlxoIconViewCellInfo *info)
{
  if (G_UNLIKELY (info->destroy != NULL))
    (*info->destroy) (info->func_data);

  free_cell_attributes (info);
  g_object_unref (G_OBJECT (info->cell));
  g_slice_free (BlxoIconViewCellInfo, info);
}



static void
blxo_icon_view_cell_layout_pack_start (GtkCellLayout   *layout,
                                      GtkCellRenderer *renderer,
                                      gboolean         expand)
{
  BlxoIconViewCellInfo *info;
  BlxoIconView         *icon_view = BLXO_ICON_VIEW (layout);

  _blxo_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  _blxo_return_if_fail (blxo_icon_view_get_cell_info (icon_view, renderer) == NULL);

  g_object_ref_sink (renderer);

  info = g_slice_new0 (BlxoIconViewCellInfo);
  info->cell = renderer;
  info->expand = expand ? TRUE : FALSE;
  info->pack = GTK_PACK_START;
  info->position = icon_view->priv->n_cells;
  info->is_text = GTK_IS_CELL_RENDERER_TEXT (renderer);

  icon_view->priv->cell_list = g_list_append (icon_view->priv->cell_list, info);
  icon_view->priv->n_cells++;

  blxo_icon_view_invalidate_sizes (icon_view);
}



static void
blxo_icon_view_cell_layout_pack_end (GtkCellLayout   *layout,
                                    GtkCellRenderer *renderer,
                                    gboolean         expand)
{
  BlxoIconViewCellInfo *info;
  BlxoIconView         *icon_view = BLXO_ICON_VIEW (layout);

  _blxo_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  _blxo_return_if_fail (blxo_icon_view_get_cell_info (icon_view, renderer) == NULL);

  g_object_ref_sink (renderer);

  info = g_slice_new0 (BlxoIconViewCellInfo);
  info->cell = renderer;
  info->expand = expand ? TRUE : FALSE;
  info->pack = GTK_PACK_END;
  info->position = icon_view->priv->n_cells;
  info->is_text = GTK_IS_CELL_RENDERER_TEXT (renderer);

  icon_view->priv->cell_list = g_list_append (icon_view->priv->cell_list, info);
  icon_view->priv->n_cells++;

  blxo_icon_view_invalidate_sizes (icon_view);
}



static void
blxo_icon_view_cell_layout_add_attribute (GtkCellLayout   *layout,
                                         GtkCellRenderer *renderer,
                                         const gchar     *attribute,
                                         gint             column)
{
  BlxoIconViewCellInfo *info;

  info = blxo_icon_view_get_cell_info (BLXO_ICON_VIEW (layout), renderer);
  if (G_LIKELY (info != NULL))
    {
      info->attributes = g_slist_prepend (info->attributes, GINT_TO_POINTER (column));
      info->attributes = g_slist_prepend (info->attributes, g_strdup (attribute));

      blxo_icon_view_invalidate_sizes (BLXO_ICON_VIEW (layout));
    }
}



static void
blxo_icon_view_cell_layout_clear (GtkCellLayout *layout)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (layout);

  g_list_foreach (icon_view->priv->cell_list, (GFunc) (void (*)(void)) free_cell_info, NULL);
  g_list_free (icon_view->priv->cell_list);
  icon_view->priv->cell_list = NULL;
  icon_view->priv->n_cells = 0;

  blxo_icon_view_invalidate_sizes (icon_view);
}



static void
blxo_icon_view_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                              GtkCellRenderer       *cell,
                                              GtkCellLayoutDataFunc  func,
                                              gpointer               func_data,
                                              GDestroyNotify         destroy)
{
  BlxoIconViewCellInfo *info;
  GDestroyNotify       notify;

  info = blxo_icon_view_get_cell_info (BLXO_ICON_VIEW (layout), cell);
  if (G_LIKELY (info != NULL))
    {
      if (G_UNLIKELY (info->destroy != NULL))
        {
          notify = info->destroy;
          info->destroy = NULL;
          (*notify) (info->func_data);
        }

      info->func = func;
      info->func_data = func_data;
      info->destroy = destroy;

      blxo_icon_view_invalidate_sizes (BLXO_ICON_VIEW (layout));
    }
}



static void
blxo_icon_view_cell_layout_clear_attributes (GtkCellLayout   *layout,
                                            GtkCellRenderer *renderer)
{
  BlxoIconViewCellInfo *info;

  info = blxo_icon_view_get_cell_info (BLXO_ICON_VIEW (layout), renderer);
  if (G_LIKELY (info != NULL))
    {
      free_cell_attributes (info);

      blxo_icon_view_invalidate_sizes (BLXO_ICON_VIEW (layout));
    }
}



static void
blxo_icon_view_cell_layout_reorder (GtkCellLayout   *layout,
                                   GtkCellRenderer *cell,
                                   gint             position)
{
  BlxoIconViewCellInfo *info;
  BlxoIconView         *icon_view = BLXO_ICON_VIEW (layout);
  GList               *lp;
  gint                 n;

  info = blxo_icon_view_get_cell_info (icon_view, cell);
  if (G_LIKELY (info != NULL))
    {
      lp = g_list_find (icon_view->priv->cell_list, info);

      icon_view->priv->cell_list = g_list_remove_link (icon_view->priv->cell_list, lp);
      icon_view->priv->cell_list = g_list_insert (icon_view->priv->cell_list, info, position);

      for (lp = icon_view->priv->cell_list, n = 0; lp != NULL; lp = lp->next, ++n)
        BLXO_ICON_VIEW_CELL_INFO (lp->data)->position = n;

      blxo_icon_view_invalidate_sizes (icon_view);
    }
}



/**
 * blxo_icon_view_new:
 *
 * Creates a new #BlxoIconView widget
 *
 * Returns: A newly created #BlxoIconView widget
 **/
GtkWidget*
blxo_icon_view_new (void)
{
  return g_object_new (BLXO_TYPE_ICON_VIEW, NULL);
}



/**
 * blxo_icon_view_new_with_model:
 * @model: The model.
 *
 * Creates a new #BlxoIconView widget with the model @model.
 *
 * Returns: A newly created #BlxoIconView widget.
 **/
GtkWidget*
blxo_icon_view_new_with_model (GtkTreeModel *model)
{
  g_return_val_if_fail (model == NULL || GTK_IS_TREE_MODEL (model), NULL);

  return g_object_new (BLXO_TYPE_ICON_VIEW,
                       "model", model,
                       NULL);
}



/**
 * blxo_icon_view_widget_to_icon_coords:
 * @icon_view: a #BlxoIconView.
 * @wx: widget x coordinate.
 * @wy: widget y coordinate.
 * @ix: return location for icon x coordinate or %NULL.
 * @iy: return location for icon y coordinate or %NULL.
 *
 * Converts widget coordinates to coordinates for the icon window
 * (the full scrollable area of the icon view).
 **/
void
blxo_icon_view_widget_to_icon_coords (const BlxoIconView *icon_view,
                                     gint               wx,
                                     gint               wy,
                                     gint              *ix,
                                     gint              *iy)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (ix != NULL))
    *ix = wx + gtk_adjustment_get_value (icon_view->priv->hadjustment);
  if (G_LIKELY (iy != NULL))
    *iy = wy + gtk_adjustment_get_value (icon_view->priv->vadjustment);
}



/**
 * blxo_icon_view_icon_to_widget_coords:
 * @icon_view : a #BlxoIconView.
 * @ix        : icon x coordinate.
 * @iy        : icon y coordinate.
 * @wx        : return location for widget x coordinate or %NULL.
 * @wy        : return location for widget y coordinate or %NULL.
 *
 * Converts icon view coordinates (coordinates in full scrollable
 * area of the icon view) to widget coordinates.
 **/
void
blxo_icon_view_icon_to_widget_coords (const BlxoIconView *icon_view,
                                     gint               ix,
                                     gint               iy,
                                     gint              *wx,
                                     gint              *wy)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (wx != NULL))
    *wx = ix - gtk_adjustment_get_value (icon_view->priv->hadjustment);
  if (G_LIKELY (wy != NULL))
    *wy = iy - gtk_adjustment_get_value (icon_view->priv->vadjustment);
}



/**
 * blxo_icon_view_get_path_at_pos:
 * @icon_view : A #BlxoIconView.
 * @x         : The x position to be identified
 * @y         : The y position to be identified
 *
 * Finds the path at the point (@x, @y), relative to widget coordinates.
 * See blxo_icon_view_get_item_at_pos(), if you are also interested in
 * the cell at the specified position.
 *
 * Returns: The #GtkTreePath corresponding to the icon or %NULL
 *          if no icon exists at that position.
 **/
GtkTreePath*
blxo_icon_view_get_path_at_pos (const BlxoIconView *icon_view,
                               gint               x,
                               gint               y)
{
  BlxoIconViewItem *item;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), NULL);

  /* translate the widget coordinates to icon window coordinates */
  x += gtk_adjustment_get_value (icon_view->priv->hadjustment);
  y += gtk_adjustment_get_value (icon_view->priv->vadjustment);

  item = blxo_icon_view_get_item_at_coords (icon_view, x, y, TRUE, NULL);

  return (item != NULL) ? gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1) : NULL;
}



/**
 * blxo_icon_view_get_item_at_pos:
 * @icon_view: A #BlxoIconView.
 * @x: The x position to be identified
 * @y: The y position to be identified
 * @path: Return location for the path, or %NULL
 * @cell: Return location for the renderer responsible for the cell
 *   at (@x, @y), or %NULL
 *
 * Finds the path at the point (@x, @y), relative to widget coordinates.
 * In contrast to blxo_icon_view_get_path_at_pos(), this function also
 * obtains the cell at the specified position. The returned path should
 * be freed with gtk_tree_path_free().
 *
 * Returns: %TRUE if an item exists at the specified position
 *
 * Since: 0.3.1
 **/
gboolean
blxo_icon_view_get_item_at_pos (const BlxoIconView *icon_view,
                               gint               x,
                               gint               y,
                               GtkTreePath      **path,
                               GtkCellRenderer  **cell)
{
  BlxoIconViewCellInfo *info = NULL;
  BlxoIconViewItem     *item;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);

  item = blxo_icon_view_get_item_at_coords (icon_view, x, y, TRUE, &info);

  if (G_LIKELY (path != NULL))
    *path = (item != NULL) ? gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1) : NULL;

  if (G_LIKELY (cell != NULL))
    *cell = (info != NULL) ? info->cell : NULL;

  return (item != NULL);
}



/**
 * blxo_icon_view_get_visible_range:
 * @icon_view  : A #BlxoIconView
 * @start_path : Return location for start of region, or %NULL
 * @end_path   : Return location for end of region, or %NULL
 *
 * Sets @start_path and @end_path to be the first and last visible path.
 * Note that there may be invisible paths in between.
 *
 * Both paths should be freed with gtk_tree_path_free() after use.
 *
 * Returns: %TRUE, if valid paths were placed in @start_path and @end_path
 *
 * Since: 0.3.1
 **/
gboolean
blxo_icon_view_get_visible_range (const BlxoIconView *icon_view,
                                 GtkTreePath      **start_path,
                                 GtkTreePath      **end_path)
{
  const BlxoIconViewPrivate *priv = icon_view->priv;
  const BlxoIconViewItem    *item;
  const GList              *lp;
  gint                      start_index = -1;
  gint                      end_index = -1;
  gint                      i;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);

  if (priv->hadjustment == NULL || priv->vadjustment == NULL)
    return FALSE;

  if (start_path == NULL && end_path == NULL)
    return FALSE;

  for (i = 0, lp = priv->items; lp != NULL; ++i, lp = lp->next)
    {
      item = (const BlxoIconViewItem *) lp->data;
      if ((item->area.x + item->area.width >= (gint) gtk_adjustment_get_value (priv->hadjustment)) &&
          (item->area.y + item->area.height >= (gint) gtk_adjustment_get_value (priv->vadjustment)) &&
          (item->area.x <= (gint) (gtk_adjustment_get_value (priv->hadjustment) + gtk_adjustment_get_page_size (priv->hadjustment))) &&
          (item->area.y <= (gint) (gtk_adjustment_get_value (priv->vadjustment) + gtk_adjustment_get_page_size (priv->vadjustment))))
        {
          if (start_index == -1)
            start_index = i;
          end_index = i;
        }
    }

  if (start_path != NULL && start_index != -1)
    *start_path = gtk_tree_path_new_from_indices (start_index, -1);
  if (end_path != NULL && end_index != -1)
    *end_path = gtk_tree_path_new_from_indices (end_index, -1);

  return (start_index != -1);
}



/**
 * blxo_icon_view_selected_foreach:
 * @icon_view : A #BlxoIconView.
 * @func      : The funcion to call for each selected icon.
 * @data      : User data to pass to the function.
 *
 * Calls a function for each selected icon. Note that the model or
 * selection cannot be modified from within this function.
 **/
void
blxo_icon_view_selected_foreach (BlxoIconView           *icon_view,
                                BlxoIconViewForeachFunc func,
                                gpointer               data)
{
  GtkTreePath *path;
  GList       *lp;

  path = gtk_tree_path_new_first ();
  for (lp = icon_view->priv->items; lp != NULL; lp = lp->next)
    {
      if (BLXO_ICON_VIEW_ITEM (lp->data)->selected)
        (*func) (icon_view, path, data);
      gtk_tree_path_next (path);
    }
  gtk_tree_path_free (path);
}



/**
 * blxo_icon_view_get_selection_mode:
 * @icon_view : A #BlxoIconView.
 *
 * Gets the selection mode of the @icon_view.
 *
 * Returns: the current selection mode
 **/
GtkSelectionMode
blxo_icon_view_get_selection_mode (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), GTK_SELECTION_SINGLE);
  return icon_view->priv->selection_mode;
}



/**
 * blxo_icon_view_set_selection_mode:
 * @icon_view : A #BlxoIconView.
 * @mode      : The selection mode
 *
 * Sets the selection mode of the @icon_view.
 **/
void
blxo_icon_view_set_selection_mode (BlxoIconView      *icon_view,
                                  GtkSelectionMode  mode)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (mode != icon_view->priv->selection_mode))
    {
      if (mode == GTK_SELECTION_NONE || icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE)
        blxo_icon_view_unselect_all (icon_view);

      icon_view->priv->selection_mode = mode;

      g_object_notify (G_OBJECT (icon_view), "selection-mode");
    }
}



/**
 * blxo_icon_view_get_layout_mode:
 * @icon_view : A #BlxoIconView.
 *
 * Returns the #BlxoIconViewLayoutMode used to layout the
 * items in the @icon_view.
 *
 * Returns: the layout mode of @icon_view.
 *
 * Since: 0.3.1.5
 **/
BlxoIconViewLayoutMode
blxo_icon_view_get_layout_mode (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), BLXO_ICON_VIEW_LAYOUT_ROWS);
  return icon_view->priv->layout_mode;
}



/**
 * blxo_icon_view_set_layout_mode:
 * @icon_view   : a #BlxoIconView.
 * @layout_mode : the new #BlxoIconViewLayoutMode for @icon_view.
 *
 * Sets the layout mode of @icon_view to @layout_mode.
 *
 * Since: 0.3.1.5
 **/
void
blxo_icon_view_set_layout_mode (BlxoIconView          *icon_view,
                               BlxoIconViewLayoutMode layout_mode)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  /* check if we have a new setting */
  if (G_LIKELY (icon_view->priv->layout_mode != layout_mode))
    {
      /* apply the new setting */
      icon_view->priv->layout_mode = layout_mode;

      /* cancel any active cell editor */
      blxo_icon_view_stop_editing (icon_view, TRUE);

      /* invalidate the current item sizes */
      blxo_icon_view_invalidate_sizes (icon_view);
      blxo_icon_view_queue_layout (icon_view);

      /* notify listeners */
      g_object_notify (G_OBJECT (icon_view), "layout-mode");
    }
}



/**
 * blxo_icon_view_get_model:
 * @icon_view : a #BlxoIconView
 *
 * Returns the model the #BlxoIconView is based on. Returns %NULL if the
 * model is unset.
 *
 * Returns: A #GtkTreeModel, or %NULL if none is currently being used.
 **/
GtkTreeModel*
blxo_icon_view_get_model (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), NULL);
  return icon_view->priv->model;
}



/**
 * blxo_icon_view_set_model:
 * @icon_view : A #BlxoIconView.
 * @model     : The model.
 *
 * Sets the model for a #BlxoIconView.
 * If the @icon_view already has a model set, it will remove
 * it before setting the new model.  If @model is %NULL, then
 * it will unset the old model.
 **/
void
blxo_icon_view_set_model (BlxoIconView  *icon_view,
                         GtkTreeModel *model)
{
  BlxoIconViewItem *item;
  GtkTreeIter      iter;
  GList           *items = NULL;
  GList           *lp;
  gint             n;

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  /* verify that we don't already use that model */
  if (G_UNLIKELY (icon_view->priv->model == model))
    return;

  /* verify the new model */
  if (G_LIKELY (model != NULL))
    {
      g_return_if_fail (gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_LIST_ONLY);

      if (G_UNLIKELY (icon_view->priv->pixbuf_column != -1))
        g_return_if_fail (gtk_tree_model_get_column_type (model, icon_view->priv->pixbuf_column) == GDK_TYPE_PIXBUF);

      if (G_UNLIKELY (icon_view->priv->icon_column != -1))
        g_return_if_fail (gtk_tree_model_get_column_type (model, icon_view->priv->icon_column) == G_TYPE_STRING);

      if (G_UNLIKELY (icon_view->priv->text_column != -1))
        g_return_if_fail (gtk_tree_model_get_column_type (model, icon_view->priv->text_column) == G_TYPE_STRING);

      if (G_UNLIKELY (icon_view->priv->markup_column != -1))
        g_return_if_fail (gtk_tree_model_get_column_type (model, icon_view->priv->markup_column) == G_TYPE_STRING);
    }

  /* be sure to cancel any pending editor */
  blxo_icon_view_stop_editing (icon_view, TRUE);

  /* disconnect from the previous model */
  if (G_LIKELY (icon_view->priv->model != NULL))
    {
      /* disconnect signals handlers from the previous model */
      g_signal_handlers_disconnect_by_func (G_OBJECT (icon_view->priv->model), blxo_icon_view_row_changed, icon_view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (icon_view->priv->model), blxo_icon_view_row_inserted, icon_view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (icon_view->priv->model), blxo_icon_view_row_deleted, icon_view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (icon_view->priv->model), blxo_icon_view_rows_reordered, icon_view);

      /* release our reference on the model */
      g_object_unref (G_OBJECT (icon_view->priv->model));

      /* drop all items belonging to the previous model */
      for (lp = icon_view->priv->items; lp != NULL; lp = lp->next)
        {
          g_free (BLXO_ICON_VIEW_ITEM (lp->data)->box);
          g_slice_free (BlxoIconViewItem, lp->data);
        }
      g_list_free (icon_view->priv->items);
      icon_view->priv->items = NULL;

      /* reset statistics */
      icon_view->priv->search_column = -1;
      icon_view->priv->anchor_item = NULL;
      icon_view->priv->cursor_item = NULL;
      icon_view->priv->prelit_item = NULL;
      icon_view->priv->last_single_clicked = NULL;
      icon_view->priv->width = 0;
      icon_view->priv->height = 0;

      /* cancel any pending single click timer */
      if (G_UNLIKELY (icon_view->priv->single_click_timeout_id != 0))
        g_source_remove (icon_view->priv->single_click_timeout_id);

      /* reset cursor when in single click mode and realized */
      if (G_UNLIKELY (icon_view->priv->single_click && gtk_widget_get_realized (GTK_WIDGET (icon_view))))
        gdk_window_set_cursor (icon_view->priv->bin_window, NULL);
    }

  /* be sure to drop any previous scroll_to_path reference,
   * as it points to the old (no longer valid) model.
   */
  if (G_UNLIKELY (icon_view->priv->scroll_to_path != NULL))
    {
      gtk_tree_row_reference_free (icon_view->priv->scroll_to_path);
      icon_view->priv->scroll_to_path = NULL;
    }

  /* activate the new model */
  icon_view->priv->model = model;

  /* connect to the new model */
  if (G_LIKELY (model != NULL))
    {
      /* take a reference on the model */
      g_object_ref (G_OBJECT (model));

      /* connect signals */
      g_signal_connect (G_OBJECT (model), "row-changed", G_CALLBACK (blxo_icon_view_row_changed), icon_view);
      g_signal_connect (G_OBJECT (model), "row-inserted", G_CALLBACK (blxo_icon_view_row_inserted), icon_view);
      g_signal_connect (G_OBJECT (model), "row-deleted", G_CALLBACK (blxo_icon_view_row_deleted), icon_view);
      g_signal_connect (G_OBJECT (model), "rows-reordered", G_CALLBACK (blxo_icon_view_rows_reordered), icon_view);

      /* check if the new model supports persistent iterators */
      if (gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_ITERS_PERSIST)
        BLXO_ICON_VIEW_SET_FLAG (icon_view, BLXO_ICON_VIEW_ITERS_PERSIST);
      else
        BLXO_ICON_VIEW_UNSET_FLAG (icon_view, BLXO_ICON_VIEW_ITERS_PERSIST);

      /* determine an appropriate search column */
      if (icon_view->priv->search_column <= 0)
        {
          /* we simply use the first string column */
          for (n = 0; n < gtk_tree_model_get_n_columns (model); ++n)
            if (g_value_type_transformable (gtk_tree_model_get_column_type (model, n), G_TYPE_STRING))
              {
                icon_view->priv->search_column = n;
                break;
              }
        }

      /* build up the initial items list */
      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          n = 0;
          do
            {
              item = g_slice_new0 (BlxoIconViewItem);
              item->iter = iter;
              item->area.width = -1;
              items = g_list_prepend (items, item);
            }
          while (gtk_tree_model_iter_next (model, &iter));
        }
      icon_view->priv->items = g_list_reverse (items);

      /* layout the new items */
      blxo_icon_view_queue_layout (icon_view);
    }

  /* hide the interactive search dialog (if any) */
  if (G_LIKELY (icon_view->priv->search_window != NULL))
    blxo_icon_view_search_dialog_hide (icon_view->priv->search_window, icon_view);

  /* notify listeners */
  g_object_notify (G_OBJECT (icon_view), "model");

  if (gtk_widget_get_realized (GTK_WIDGET (icon_view)))
    gtk_widget_queue_resize (GTK_WIDGET (icon_view));
}



static void
update_text_cell (BlxoIconView *icon_view)
{
  BlxoIconViewCellInfo *info;
  GList *l;
  gint i;

  if (icon_view->priv->text_column == -1 &&
      icon_view->priv->markup_column == -1)
    {
      if (icon_view->priv->text_cell != -1)
        {
          info = g_list_nth_data (icon_view->priv->cell_list,
                                  icon_view->priv->text_cell);

          icon_view->priv->cell_list = g_list_remove (icon_view->priv->cell_list, info);

          free_cell_info (info);

          icon_view->priv->n_cells--;
          icon_view->priv->text_cell = -1;
        }
    }
  else
    {
      if (icon_view->priv->text_cell == -1)
        {
          GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
          gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (icon_view), cell, FALSE);
          for (l = icon_view->priv->cell_list, i = 0; l; l = l->next, i++)
            {
              info = l->data;
              if (info->cell == cell)
                {
                  icon_view->priv->text_cell = i;
                  break;
                }
            }
        }

      info = g_list_nth_data (icon_view->priv->cell_list,
                              icon_view->priv->text_cell);

      if (icon_view->priv->markup_column != -1)
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
                                        info->cell,
                                        "markup", icon_view->priv->markup_column,
                                        NULL);
      else
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
                                        info->cell,
                                        "text", icon_view->priv->text_column,
                                        NULL);
    }
}

static void
update_pixbuf_cell (BlxoIconView *icon_view)
{
  BlxoIconViewCellInfo *info;
  GList *l;
  gint i;

  if (icon_view->priv->pixbuf_column == -1 &&
      icon_view->priv->icon_column == -1)
    {
      if (icon_view->priv->pixbuf_cell != -1)
        {
          info = g_list_nth_data (icon_view->priv->cell_list,
                                  icon_view->priv->pixbuf_cell);

          icon_view->priv->cell_list = g_list_remove (icon_view->priv->cell_list, info);

          free_cell_info (info);

          icon_view->priv->n_cells--;
          icon_view->priv->pixbuf_cell = -1;
        }
    }
  else
    {
      if (icon_view->priv->pixbuf_cell == -1)
        {
          GtkCellRenderer *cell;

          if (icon_view->priv->pixbuf_column != -1)
            {
              cell = gtk_cell_renderer_pixbuf_new ();
            }
          else
            {
              cell = blxo_cell_renderer_icon_new ();
            }

          gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_view), cell, FALSE);
          for (l = icon_view->priv->cell_list, i = 0; l; l = l->next, i++)
            {
              info = l->data;
              if (info->cell == cell)
                {
                  icon_view->priv->pixbuf_cell = i;
                  break;
                }
            }
        }

      info = g_list_nth_data (icon_view->priv->cell_list,
                              icon_view->priv->pixbuf_cell);

      if (icon_view->priv->pixbuf_column != -1)
        {
          gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
                                          info->cell,
                                          "pixbuf", icon_view->priv->pixbuf_column,
                                          NULL);
        }
      else
        {
          gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
                                          info->cell,
                                          "icon", icon_view->priv->icon_column,
                                          NULL);
        }
    }
}



/**
 * blxo_icon_view_select_path:
 * @icon_view : A #BlxoIconView.
 * @path      : The #GtkTreePath to be selected.
 *
 * Selects the row at @path.
 **/
void
blxo_icon_view_select_path (BlxoIconView *icon_view,
                           GtkTreePath *path)
{
  BlxoIconViewItem *item;

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (icon_view->priv->model != NULL);
  g_return_if_fail (gtk_tree_path_get_depth (path) > 0);

  item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);
  if (G_LIKELY (item != NULL))
    blxo_icon_view_select_item (icon_view, item);
}



/**
 * blxo_icon_view_unselect_path:
 * @icon_view : A #BlxoIconView.
 * @path      : The #GtkTreePath to be unselected.
 *
 * Unselects the row at @path.
 **/
void
blxo_icon_view_unselect_path (BlxoIconView *icon_view,
                             GtkTreePath *path)
{
  BlxoIconViewItem *item;

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (icon_view->priv->model != NULL);
  g_return_if_fail (gtk_tree_path_get_depth (path) > 0);

  item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);
  if (G_LIKELY (item != NULL))
    blxo_icon_view_unselect_item (icon_view, item);
}



/**
 * blxo_icon_view_get_selected_items:
 * @icon_view: A #BlxoIconView.
 *
 * Creates a list of paths of all selected items. Additionally, if you are
 * planning on modifying the model after calling this function, you may
 * want to convert the returned list into a list of #GtkTreeRowReference<!-- -->s.
 * To do this, you can use gtk_tree_row_reference_new().
 *
 * To free the return value, use:
 * <informalexample><programlisting>
 * g_list_foreach (list, gtk_tree_path_free, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Returns: A #GList containing a #GtkTreePath for each selected row.
 **/
GList*
blxo_icon_view_get_selected_items (const BlxoIconView *icon_view)
{
  GList *selected = NULL;
  GList *lp;
  gint   i;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), NULL);

  for (i = 0, lp = icon_view->priv->items; lp != NULL; ++i, lp = lp->next)
    {
      if (BLXO_ICON_VIEW_ITEM (lp->data)->selected)
        selected = g_list_prepend (selected, gtk_tree_path_new_from_indices (i, -1));
    }

  return g_list_reverse (selected);
}



/**
 * blxo_icon_view_select_all:
 * @icon_view : A #BlxoIconView.
 *
 * Selects all the icons. @icon_view must has its selection mode set
 * to #GTK_SELECTION_MULTIPLE.
 **/
void
blxo_icon_view_select_all (BlxoIconView *icon_view)
{
  GList *items;
  gboolean dirty = FALSE;

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    return;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      BlxoIconViewItem *item = items->data;

      if (!item->selected)
        {
          dirty = TRUE;
          item->selected = TRUE;
          blxo_icon_view_queue_draw_item (icon_view, item);
        }
    }

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}



/**
 * blxo_icon_view_selection_invert:
 * @icon_view : A #BlxoIconView.
 *
 * Selects all the icons that are currently not selected. @icon_view must
 * has its selection mode set to #GTK_SELECTION_MULTIPLE.
 **/
void
blxo_icon_view_selection_invert (BlxoIconView *icon_view)
{
  GList           *items;
  gboolean         dirty = FALSE;
  BlxoIconViewItem *item;

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    return;

  for (items = icon_view->priv->items; items; items = items->next)
    {
      item = items->data;

      item->selected = !item->selected;
      blxo_icon_view_queue_draw_item (icon_view, item);

      dirty = TRUE;
    }

  if (dirty)
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}



/**
 * blxo_icon_view_unselect_all:
 * @icon_view : A #BlxoIconView.
 *
 * Unselects all the icons.
 **/
void
blxo_icon_view_unselect_all (BlxoIconView *icon_view)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_UNLIKELY (icon_view->priv->selection_mode == GTK_SELECTION_BROWSE))
    return;

  if (blxo_icon_view_unselect_all_internal (icon_view))
    g_signal_emit (icon_view, icon_view_signals[SELECTION_CHANGED], 0);
}



/**
 * blxo_icon_view_path_is_selected:
 * @icon_view: A #BlxoIconView.
 * @path: A #GtkTreePath to check selection on.
 *
 * Returns %TRUE if the icon pointed to by @path is currently
 * selected. If @icon does not point to a valid location, %FALSE is returned.
 *
 * Returns: %TRUE if @path is selected.
 **/
gboolean
blxo_icon_view_path_is_selected (const BlxoIconView *icon_view,
                                GtkTreePath       *path)
{
  BlxoIconViewItem *item;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (icon_view->priv->model != NULL, FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);

  return (item != NULL && item->selected);
}



/**
 * blxo_icon_view_item_activated:
 * @icon_view : a #BlxoIconView
 * @path      : the #GtkTreePath to be activated
 *
 * Activates the item determined by @path.
 **/
void
blxo_icon_view_item_activated (BlxoIconView *icon_view,
                              GtkTreePath *path)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (gtk_tree_path_get_depth (path) > 0);

  g_signal_emit (icon_view, icon_view_signals[ITEM_ACTIVATED], 0, path);
}



/**
 * blxo_icon_view_get_item_column:
 * @icon_view : A #BlxoIconView.
 * @path      : The #GtkTreePath of the item.
 *
 * Gets the column in which the item @path is currently
 * displayed. Column numbers start at 0.
 *
 * Returns: The column in which the item is displayed
 *
 * Since: 0.7.1
 **/
gint
blxo_icon_view_get_item_column (BlxoIconView *icon_view,
                               GtkTreePath *path)
{
  BlxoIconViewItem *item;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  g_return_val_if_fail (icon_view->priv->model != NULL, -1);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, -1);

  item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);
  if (G_LIKELY (item != NULL))
    return item->col;

  return -1;
}



/**
 * blxo_icon_view_get_item_row:
 * @icon_view : A #BlxoIconView.
 * @path      : The #GtkTreePath of the item.
 *
 * Gets the row in which the item @path is currently
 * displayed. Row numbers start at 0.
 *
 * Returns: The row in which the item is displayed
 *
 * Since: 0.7.1
 */
gint
blxo_icon_view_get_item_row (BlxoIconView *icon_view,
                            GtkTreePath *path)
{
  BlxoIconViewItem *item;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  g_return_val_if_fail (icon_view->priv->model != NULL, -1);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, -1);

  item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);
  if (G_LIKELY (item != NULL))
    return item->row;

  return -1;
}



/**
 * blxo_icon_view_get_cursor:
 * @icon_view : A #BlxoIconView
 * @path      : Return location for the current cursor path, or %NULL
 * @cell      : Return location the current focus cell, or %NULL
 *
 * Fills in @path and @cell with the current cursor path and cell.
 * If the cursor isn't currently set, then *@path will be %NULL.
 * If no cell currently has focus, then *@cell will be %NULL.
 *
 * The returned #GtkTreePath must be freed with gtk_tree_path_free().
 *
 * Returns: %TRUE if the cursor is set.
 *
 * Since: 0.3.1
 **/
gboolean
blxo_icon_view_get_cursor (const BlxoIconView *icon_view,
                          GtkTreePath      **path,
                          GtkCellRenderer  **cell)
{
  BlxoIconViewCellInfo *info;
  BlxoIconViewItem     *item;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);

  item = icon_view->priv->cursor_item;
  info = (icon_view->priv->cursor_cell < 0) ? NULL : g_list_nth_data (icon_view->priv->cell_list, icon_view->priv->cursor_cell);

  if (G_LIKELY (path != NULL))
    *path = (item != NULL) ? gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1) : NULL;

  if (G_LIKELY (cell != NULL))
    *cell = (info != NULL) ? info->cell : NULL;

  return (item != NULL);
}



/**
 * blxo_icon_view_set_cursor:
 * @icon_view     : a #BlxoIconView
 * @path          : a #GtkTreePath
 * @cell          : a #GtkCellRenderer or %NULL
 * @start_editing : %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user's attention on a particular item.
 * If @cell is not %NULL, then focus is given to the cell specified by
 * it. Additionally, if @start_editing is %TRUE, then editing should be
 * started in the specified cell.
 *
 * This function is often followed by <literal>gtk_widget_grab_focus
 * (icon_view)</literal> in order to give keyboard focus to the widget.
 * Please note that editing can only happen when the widget is realized.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_set_cursor (BlxoIconView     *icon_view,
                          GtkTreePath     *path,
                          GtkCellRenderer *cell,
                          gboolean         start_editing)
{
  BlxoIconViewItem *item;
  BlxoIconViewCellInfo *info =  NULL;
  GList *l;
  gint i, cell_pos;

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell));

  blxo_icon_view_stop_editing (icon_view, TRUE);

  item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);
  if (G_UNLIKELY (item == NULL))
    return;

  cell_pos = -1;
  for (l = icon_view->priv->cell_list, i = 0; l; l = l->next, i++)
    {
      info = l->data;

      if (info->cell == cell)
        {
          cell_pos = i;
          break;
        }

      info = NULL;
    }

  /* place the cursor on the item */
  blxo_icon_view_set_cursor_item (icon_view, item, cell_pos);
  icon_view->priv->anchor_item = item;

  /* scroll to the item (maybe delayed) */
  blxo_icon_view_scroll_to_path (icon_view, path, FALSE, 0.0f, 0.0f);

  if (!info)
    return;

  if (start_editing)
    blxo_icon_view_start_editing (icon_view, item, info, NULL);
}



/**
 * blxo_icon_view_scroll_to_path:
 * @icon_view: A #BlxoIconView.
 * @path: The path of the item to move to.
 * @use_align: whether to use alignment arguments, or %FALSE.
 * @row_align: The vertical alignment of the item specified by @path.
 * @col_align: The horizontal alignment of the item specified by @column.
 *
 * Moves the alignments of @icon_view to the position specified by @path.
 * @row_align determines where the row is placed, and @col_align determines where
 * @column is placed.  Both are expected to be between 0.0 and 1.0.
 * 0.0 means left/top alignment, 1.0 means right/bottom alignment, 0.5 means center.
 *
 * If @use_align is %FALSE, then the alignment arguments are ignored, and the
 * tree does the minimum amount of work to scroll the item onto the screen.
 * This means that the item will be scrolled to the edge closest to its current
 * position.  If the item is currently visible on the screen, nothing is done.
 *
 * This function only works if the model is set, and @path is a valid row on the
 * model.  If the model changes before the @tree_view is realized, the centered
 * path will be modified to reflect this change.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_scroll_to_path (BlxoIconView *icon_view,
                              GtkTreePath *path,
                              gboolean     use_align,
                              gfloat       row_align,
                              gfloat       col_align)
{
  BlxoIconViewItem *item;
  GtkAllocation    allocation;

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (gtk_tree_path_get_depth (path) > 0);
  g_return_if_fail (row_align >= 0.0 && row_align <= 1.0);
  g_return_if_fail (col_align >= 0.0 && col_align <= 1.0);

  gtk_widget_get_allocation (GTK_WIDGET (icon_view), &allocation);

  /* Delay scrolling if either not realized or pending layout() */
  if (!gtk_widget_get_realized (GTK_WIDGET (icon_view)) || icon_view->priv->layout_idle_id != 0)
    {
      /* release the previous scroll_to_path reference */
      if (G_UNLIKELY (icon_view->priv->scroll_to_path != NULL))
        gtk_tree_row_reference_free (icon_view->priv->scroll_to_path);

      /* remember a reference for the new path and settings */
      icon_view->priv->scroll_to_path = gtk_tree_row_reference_new_proxy (G_OBJECT (icon_view), icon_view->priv->model, path);
      icon_view->priv->scroll_to_use_align = use_align;
      icon_view->priv->scroll_to_row_align = row_align;
      icon_view->priv->scroll_to_col_align = col_align;
    }
  else
    {
      item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);
      if (G_UNLIKELY (item == NULL))
        return;

      if (use_align)
        {
          gint x, y;
          gint focus_width;
          gfloat offset, value;

          gtk_widget_style_get (GTK_WIDGET (icon_view),
                                "focus-line-width", &focus_width,
                                NULL);

          gdk_window_get_position (icon_view->priv->bin_window, &x, &y);

          offset =  y + item->area.y - focus_width -
            row_align * (allocation.height - item->area.height);
          value = CLAMP (gtk_adjustment_get_value (icon_view->priv->vadjustment) + offset,
                         gtk_adjustment_get_lower (icon_view->priv->vadjustment),
                         gtk_adjustment_get_upper (icon_view->priv->vadjustment) - gtk_adjustment_get_page_size (icon_view->priv->vadjustment));
          gtk_adjustment_set_value (icon_view->priv->vadjustment, value);

          offset = x + item->area.x - focus_width -
            col_align * (allocation.width - item->area.width);
          value = CLAMP (gtk_adjustment_get_value (icon_view->priv->hadjustment) + offset,
                         gtk_adjustment_get_lower (icon_view->priv->hadjustment),
                         gtk_adjustment_get_upper (icon_view->priv->hadjustment) - gtk_adjustment_get_page_size (icon_view->priv->hadjustment));
          gtk_adjustment_set_value (icon_view->priv->hadjustment, value);

          /* Prior to GTK 3.18, we need to emit "changed" ourselves */
#if !GTK_CHECK_VERSION (3, 18, 0)
          gtk_adjustment_changed (icon_view->priv->hadjustment);
          gtk_adjustment_changed (icon_view->priv->vadjustment);
#endif
        }
      else
        {
          blxo_icon_view_scroll_to_item (icon_view, item);
        }
    }
}



/**
 * blxo_icon_view_get_orientation:
 * @icon_view : a #BlxoIconView
 *
 * Returns the value of the ::orientation property which determines
 * whether the labels are drawn beside the icons instead of below.
 *
 * Returns: the relative position of texts and icons
 *
 * Since: 0.3.1
 **/
GtkOrientation
blxo_icon_view_get_orientation (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), GTK_ORIENTATION_VERTICAL);
  return icon_view->priv->orientation;
}



/**
 * blxo_icon_view_set_orientation:
 * @icon_view   : a #BlxoIconView
 * @orientation : the relative position of texts and icons
 *
 * Sets the ::orientation property which determines whether the labels
 * are drawn beside the icons instead of below.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_set_orientation (BlxoIconView   *icon_view,
                               GtkOrientation orientation)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (icon_view->priv->orientation != orientation))
    {
      icon_view->priv->orientation = orientation;

      blxo_icon_view_stop_editing (icon_view, TRUE);
      blxo_icon_view_invalidate_sizes (icon_view);

      update_text_cell (icon_view);
      update_pixbuf_cell (icon_view);

      g_object_notify (G_OBJECT (icon_view), "orientation");
    }
}



/**
 * blxo_icon_view_get_columns:
 * @icon_view: a #BlxoIconView
 *
 * Returns the value of the ::columns property.
 *
 * Returns: the number of columns, or -1
 */
gint
blxo_icon_view_get_columns (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  return icon_view->priv->columns;
}



/**
 * blxo_icon_view_set_columns:
 * @icon_view : a #BlxoIconView
 * @columns   : the number of columns
 *
 * Sets the ::columns property which determines in how
 * many columns the icons are arranged. If @columns is
 * -1, the number of columns will be chosen automatically
 * to fill the available area.
 *
 * Since: 0.3.1
 */
void
blxo_icon_view_set_columns (BlxoIconView *icon_view,
                           gint         columns)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (icon_view->priv->columns != columns))
    {
      icon_view->priv->columns = columns;

      blxo_icon_view_stop_editing (icon_view, TRUE);
      blxo_icon_view_queue_layout (icon_view);

      g_object_notify (G_OBJECT (icon_view), "columns");
    }
}



/**
 * blxo_icon_view_get_item_width:
 * @icon_view: a #BlxoIconView
 *
 * Returns the value of the ::item-width property.
 *
 * Returns: the width of a single item, or -1
 *
 * Since: 0.3.1
 */
gint
blxo_icon_view_get_item_width (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  return icon_view->priv->item_width;
}



/**
 * blxo_icon_view_set_item_width:
 * @icon_view  : a #BlxoIconView
 * @item_width : the width for each item
 *
 * Sets the ::item-width property which specifies the width
 * to use for each item. If it is set to -1, the icon view will
 * automatically determine a suitable item size.
 *
 * Since: 0.3.1
 */
void
blxo_icon_view_set_item_width (BlxoIconView *icon_view,
                              gint         item_width)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->item_width != item_width)
    {
      icon_view->priv->item_width = item_width;

      blxo_icon_view_stop_editing (icon_view, TRUE);
      blxo_icon_view_invalidate_sizes (icon_view);

      update_text_cell (icon_view);

      g_object_notify (G_OBJECT (icon_view), "item-width");
    }
}



/**
 * blxo_icon_view_get_spacing:
 * @icon_view: a #BlxoIconView
 *
 * Returns the value of the ::spacing property.
 *
 * Returns: the space between cells
 *
 * Since: 0.3.1
 */
gint
blxo_icon_view_get_spacing (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  return icon_view->priv->spacing;
}



/**
 * blxo_icon_view_set_spacing:
 * @icon_view : a #BlxoIconView
 * @spacing   : the spacing
 *
 * Sets the ::spacing property which specifies the space
 * which is inserted between the cells (i.e. the icon and
 * the text) of an item.
 *
 * Since: 0.3.1
 */
void
blxo_icon_view_set_spacing (BlxoIconView *icon_view,
                           gint         spacing)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (icon_view->priv->spacing != spacing))
    {
      icon_view->priv->spacing = spacing;

      blxo_icon_view_stop_editing (icon_view, TRUE);
      blxo_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "spacing");
    }
}



/**
 * blxo_icon_view_get_row_spacing:
 * @icon_view: a #BlxoIconView
 *
 * Returns the value of the ::row-spacing property.
 *
 * Returns: the space between rows
 *
 * Since: 0.3.1
 */
gint
blxo_icon_view_get_row_spacing (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  return icon_view->priv->row_spacing;
}



/**
 * blxo_icon_view_set_row_spacing:
 * @icon_view   : a #BlxoIconView
 * @row_spacing : the row spacing
 *
 * Sets the ::row-spacing property which specifies the space
 * which is inserted between the rows of the icon view.
 *
 * Since: 0.3.1
 */
void
blxo_icon_view_set_row_spacing (BlxoIconView *icon_view,
                               gint         row_spacing)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (icon_view->priv->row_spacing != row_spacing))
    {
      icon_view->priv->row_spacing = row_spacing;

      blxo_icon_view_stop_editing (icon_view, TRUE);
      blxo_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "row-spacing");
    }
}



/**
 * blxo_icon_view_get_column_spacing:
 * @icon_view: a #BlxoIconView
 *
 * Returns the value of the ::column-spacing property.
 *
 * Returns: the space between columns
 *
 * Since: 0.3.1
 **/
gint
blxo_icon_view_get_column_spacing (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  return icon_view->priv->column_spacing;
}



/**
 * blxo_icon_view_set_column_spacing:
 * @icon_view      : a #BlxoIconView
 * @column_spacing : the column spacing
 *
 * Sets the ::column-spacing property which specifies the space
 * which is inserted between the columns of the icon view.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_set_column_spacing (BlxoIconView *icon_view,
                                  gint         column_spacing)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (icon_view->priv->column_spacing != column_spacing))
    {
      icon_view->priv->column_spacing = column_spacing;

      blxo_icon_view_stop_editing (icon_view, TRUE);
      blxo_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "column-spacing");
    }
}



/**
 * blxo_icon_view_get_margin:
 * @icon_view : a #BlxoIconView
 *
 * Returns the value of the ::margin property.
 *
 * Returns: the space at the borders
 *
 * Since: 0.3.1
 **/
gint
blxo_icon_view_get_margin (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  return icon_view->priv->margin;
}



/**
 * blxo_icon_view_set_margin:
 * @icon_view : a #BlxoIconView
 * @margin    : the margin
 *
 * Sets the ::margin property which specifies the space
 * which is inserted at the top, bottom, left and right
 * of the icon view.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_set_margin (BlxoIconView *icon_view,
                          gint         margin)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (G_LIKELY (icon_view->priv->margin != margin))
    {
      icon_view->priv->margin = margin;

      blxo_icon_view_stop_editing (icon_view, TRUE);
      blxo_icon_view_invalidate_sizes (icon_view);

      g_object_notify (G_OBJECT (icon_view), "margin");
    }
}



/* Get/set whether drag_motion requested the drag data and
 * drag_data_received should thus not actually insert the data,
 * since the data doesn't result from a drop.
 */
static void
set_status_pending (GdkDragContext *context,
                    GdkDragAction   suggested_action)
{
  g_object_set_data (G_OBJECT (context),
                     I_("blxo-icon-view-status-pending"),
                     GINT_TO_POINTER (suggested_action));
}

static GdkDragAction
get_status_pending (GdkDragContext *context)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (context), I_("blxo-icon-view-status-pending")));
}

static void
unset_reorderable (BlxoIconView *icon_view)
{
  if (icon_view->priv->reorderable)
    {
      icon_view->priv->reorderable = FALSE;
      g_object_notify (G_OBJECT (icon_view), "reorderable");
    }
}

static void
clear_source_info (BlxoIconView *icon_view)
{
  if (icon_view->priv->source_targets)
    gtk_target_list_unref (icon_view->priv->source_targets);
  icon_view->priv->source_targets = NULL;

  icon_view->priv->source_set = FALSE;
}

static void
clear_dest_info (BlxoIconView *icon_view)
{
  if (icon_view->priv->dest_targets)
    gtk_target_list_unref (icon_view->priv->dest_targets);
  icon_view->priv->dest_targets = NULL;

  icon_view->priv->dest_set = FALSE;
}

static void
set_source_row (GdkDragContext *context,
                GtkTreeModel   *model,
                GtkTreePath    *source_row)
{
  if (source_row)
    g_object_set_data_full (G_OBJECT (context),
                            I_("blxo-icon-view-source-row"),
                            gtk_tree_row_reference_new (model, source_row),
                            (GDestroyNotify) gtk_tree_row_reference_free);
  else
    g_object_set_data_full (G_OBJECT (context),
                            I_("blxo-icon-view-source-row"),
                            NULL, NULL);
}

static GtkTreePath*
get_source_row (GdkDragContext *context)
{
  GtkTreeRowReference *ref;

  ref = g_object_get_data (G_OBJECT (context), I_("blxo-icon-view-source-row"));

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}

typedef struct
{
  GtkTreeRowReference *dest_row;
  gboolean             empty_view_drop;
  gboolean             drop_append_mode;
} DestRow;

static void
dest_row_free (gpointer data)
{
  DestRow *dr = (DestRow *)data;

  gtk_tree_row_reference_free (dr->dest_row);
  g_slice_free (DestRow, dr);
}

static void
set_dest_row (GdkDragContext *context,
              GtkTreeModel   *model,
              GtkTreePath    *dest_row,
              gboolean        empty_view_drop,
              gboolean        drop_append_mode)
{
  DestRow *dr;

  if (!dest_row)
    {
      g_object_set_data_full (G_OBJECT (context),
                              I_("blxo-icon-view-dest-row"),
                              NULL, NULL);
      return;
    }

  dr = g_slice_new0 (DestRow);

  dr->dest_row = gtk_tree_row_reference_new (model, dest_row);
  dr->empty_view_drop = empty_view_drop;
  dr->drop_append_mode = drop_append_mode;
  g_object_set_data_full (G_OBJECT (context),
                          I_("blxo-icon-view-dest-row"),
                          dr, (GDestroyNotify) dest_row_free);
}



static GtkTreePath*
get_dest_row (GdkDragContext *context)
{
  DestRow *dr;

  dr = g_object_get_data (G_OBJECT (context), I_("blxo-icon-view-dest-row"));

  if (dr)
    {
      GtkTreePath *path = NULL;

      if (dr->dest_row)
        path = gtk_tree_row_reference_get_path (dr->dest_row);
      else if (dr->empty_view_drop)
        path = gtk_tree_path_new_from_indices (0, -1);
      else
        path = NULL;

      if (path && dr->drop_append_mode)
        gtk_tree_path_next (path);

      return path;
    }
  else
    return NULL;
}



static gboolean
check_model_dnd (GtkTreeModel *model,
                 GType         required_iface,
                 const gchar  *_signal)
{
  if (model == NULL || !G_TYPE_CHECK_INSTANCE_TYPE ((model), required_iface))
    {
      g_warning ("You must override the default '%s' handler "
                 "on BlxoIconView when using models that don't support "
                 "the %s interface and enabling drag-and-drop. The simplest way to do this "
                 "is to connect to '%s' and call "
                 "g_signal_stop_emission_by_name() in your signal handler to prevent "
                 "the default handler from running. Look at the source code "
                 "for the default handler in gtkiconview.c to get an idea what "
                 "your handler should do. (gtkiconview.c is in the GTK+ source "
                 "code.) If you're using GTK+ from a language other than C, "
                 "there may be a more natural way to override default handlers, e.g. via derivation.",
                 _signal, g_type_name (required_iface), _signal);
      return FALSE;
    }
  else
    return TRUE;
}



static void
remove_scroll_timeout (BlxoIconView *icon_view)
{
  if (icon_view->priv->scroll_timeout_id != 0)
    {
      g_source_remove (icon_view->priv->scroll_timeout_id);

      icon_view->priv->scroll_timeout_id = 0;
    }
}



static void
blxo_icon_view_autoscroll (BlxoIconView *icon_view)
{
  gint px, py, x, y, width, height;
  gint hoffset, voffset;
  gfloat value;

#if GTK_CHECK_VERSION (3, 16, 0)
  GdkSeat          *seat;
  GdkDevice        *pointer_dev;

  seat = gdk_display_get_default_seat (gdk_window_get_display (gtk_widget_get_window (GTK_WIDGET (icon_view))));
  pointer_dev = gdk_seat_get_pointer (seat);

  gdk_window_get_device_position (gtk_widget_get_window (GTK_WIDGET (icon_view)), pointer_dev, &px, &py, NULL);
  gdk_window_get_geometry (gtk_widget_get_window (GTK_WIDGET (icon_view)), &x, &y, &width, &height);
#else
  gdk_window_get_pointer (gtk_widget_get_window (GTK_WIDGET (icon_view)), &px, &py, NULL);
  gdk_window_get_geometry (gtk_widget_get_window (GTK_WIDGET (icon_view)), &x, &y, &width, &height, NULL);
#endif

  /* see if we are near the edge. */
  voffset = py - (y + 2 * SCROLL_EDGE_SIZE);
  if (voffset > 0)
    voffset = MAX (py - (y + height - 2 * SCROLL_EDGE_SIZE), 0);

  hoffset = px - (x + 2 * SCROLL_EDGE_SIZE);
  if (hoffset > 0)
    hoffset = MAX (px - (x + width - 2 * SCROLL_EDGE_SIZE), 0);

  if (voffset != 0)
    {
      value = CLAMP (gtk_adjustment_get_value (icon_view->priv->vadjustment) + voffset,
                     gtk_adjustment_get_lower (icon_view->priv->vadjustment),
                     gtk_adjustment_get_upper (icon_view->priv->vadjustment) - gtk_adjustment_get_page_size (icon_view->priv->vadjustment));
      gtk_adjustment_set_value (icon_view->priv->vadjustment, value);
    }
  if (hoffset != 0)
    {
      value = CLAMP (gtk_adjustment_get_value (icon_view->priv->hadjustment) + hoffset,
                     gtk_adjustment_get_lower (icon_view->priv->hadjustment),
                     gtk_adjustment_get_upper (icon_view->priv->hadjustment) - gtk_adjustment_get_page_size (icon_view->priv->hadjustment));
      gtk_adjustment_set_value (icon_view->priv->hadjustment, value);
    }
}


static gboolean
drag_scroll_timeout (gpointer data)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (data);

  blxo_icon_view_autoscroll (icon_view);

  return TRUE;
}


static gboolean
set_destination (BlxoIconView    *icon_view,
                 GdkDragContext *context,
                 gint            x,
                 gint            y,
                 GdkDragAction  *suggested_action,
                 GdkAtom        *target)
{
  GtkWidget *widget;
  GtkTreePath *path = NULL;
  BlxoIconViewDropPosition pos;
  BlxoIconViewDropPosition old_pos;
  GtkTreePath *old_dest_path = NULL;
  gboolean can_drop = FALSE;

  widget = GTK_WIDGET (icon_view);

  *suggested_action = 0;
  *target = GDK_NONE;

  if (!icon_view->priv->dest_set)
    {
      /* someone unset us as a drag dest, note that if
       * we return FALSE drag_leave isn't called
       */

      blxo_icon_view_set_drag_dest_item (icon_view,
                                        NULL,
                                        BLXO_ICON_VIEW_DROP_LEFT);

      remove_scroll_timeout (BLXO_ICON_VIEW (widget));

      return FALSE; /* no longer a drop site */
    }

  *target = gtk_drag_dest_find_target (widget, context, icon_view->priv->dest_targets);
  if (*target == GDK_NONE)
    return FALSE;

  if (!blxo_icon_view_get_dest_item_at_pos (icon_view, x, y, &path, &pos))
    {
      gint n_children;
      GtkTreeModel *model;

      /* the row got dropped on empty space, let's setup a special case
       */

      if (path)
        gtk_tree_path_free (path);

      model = blxo_icon_view_get_model (icon_view);

      n_children = gtk_tree_model_iter_n_children (model, NULL);
      if (n_children)
        {
          pos = BLXO_ICON_VIEW_DROP_BELOW;
          path = gtk_tree_path_new_from_indices (n_children - 1, -1);
        }
      else
        {
          pos = BLXO_ICON_VIEW_DROP_ABOVE;
          path = gtk_tree_path_new_from_indices (0, -1);
        }

      can_drop = TRUE;

      goto out;
    }

  g_assert (path);

  blxo_icon_view_get_drag_dest_item (icon_view,
                                    &old_dest_path,
                                    &old_pos);

  if (old_dest_path)
    gtk_tree_path_free (old_dest_path);

  if (TRUE /* FIXME if the location droppable predicate */)
    {
      can_drop = TRUE;
    }

out:
  if (can_drop)
    {
      GtkWidget *source_widget;

      *suggested_action = gdk_drag_context_get_suggested_action (context);
      source_widget = gtk_drag_get_source_widget (context);

      if (source_widget == widget)
        {
          /* Default to MOVE, unless the user has
           * pressed ctrl or shift to affect available actions
           */
          if ((gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
            *suggested_action = GDK_ACTION_MOVE;
        }

      blxo_icon_view_set_drag_dest_item (BLXO_ICON_VIEW (widget),
                                        path, pos);
    }
  else
    {
      /* can't drop here */
      blxo_icon_view_set_drag_dest_item (BLXO_ICON_VIEW (widget),
                                        NULL,
                                        BLXO_ICON_VIEW_DROP_LEFT);
    }

  if (path)
    gtk_tree_path_free (path);

  return TRUE;
}

static GtkTreePath*
get_logical_destination (BlxoIconView *icon_view,
                         gboolean    *drop_append_mode)
{
  /* adjust path to point to the row the drop goes in front of */
  GtkTreePath *path = NULL;
  BlxoIconViewDropPosition pos;

  *drop_append_mode = FALSE;

  blxo_icon_view_get_drag_dest_item (icon_view, &path, &pos);

  if (path == NULL)
    return NULL;

  if (pos == BLXO_ICON_VIEW_DROP_RIGHT ||
      pos == BLXO_ICON_VIEW_DROP_BELOW)
    {
      GtkTreeIter iter;
      GtkTreeModel *model = icon_view->priv->model;

      if (!gtk_tree_model_get_iter (model, &iter, path) ||
          !gtk_tree_model_iter_next (model, &iter))
        *drop_append_mode = TRUE;
      else
        {
          *drop_append_mode = FALSE;
          gtk_tree_path_next (path);
        }
    }

  return path;
}

static gboolean
blxo_icon_view_maybe_begin_drag (BlxoIconView    *icon_view,
                                GdkEventMotion *event)
{
  GdkDragContext *context;
  GtkTreePath *path = NULL;
  gint button;
  GtkTreeModel *model;
  gboolean retval = FALSE;

  if (!icon_view->priv->source_set)
    goto out;

  if (icon_view->priv->pressed_button < 0)
    goto out;

  if (!gtk_drag_check_threshold (GTK_WIDGET (icon_view),
                                 icon_view->priv->press_start_x,
                                 icon_view->priv->press_start_y,
                                 event->x, event->y))
    goto out;

  model = blxo_icon_view_get_model (icon_view);

  if (model == NULL)
    goto out;

  button = icon_view->priv->pressed_button;
  icon_view->priv->pressed_button = -1;

  path = blxo_icon_view_get_path_at_pos (icon_view,
                                        icon_view->priv->press_start_x,
                                        icon_view->priv->press_start_y);

  if (path == NULL)
    goto out;

  if (!GTK_IS_TREE_DRAG_SOURCE (model) ||
      !gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (model),
                                           path))
    goto out;

  /* FIXME Check whether we're a start button, if not return FALSE and
   * free path
   */

  /* Now we can begin the drag */

  retval = TRUE;

#if GTK_CHECK_VERSION (3, 10, 0)
  context = gtk_drag_begin_with_coordinates (GTK_WIDGET (icon_view),
                                             icon_view->priv->source_targets,
                                             icon_view->priv->source_actions,
                                             button,
                                             (GdkEvent *)event,
                                             event->x, event->y);
#else
  context = gtk_drag_begin (GTK_WIDGET (icon_view),
                            icon_view->priv->source_targets,
                            icon_view->priv->source_actions,
                            button,
                            (GdkEvent*)event);
#endif

  set_source_row (context, model, path);

 out:
  if (path)
    gtk_tree_path_free (path);

  return retval;
}

/* Source side drag signals */
static void
blxo_icon_view_drag_begin (GtkWidget      *widget,
                          GdkDragContext *context)
{
  BlxoIconView *icon_view;
  BlxoIconViewItem *item;
#if GTK_CHECK_VERSION (3, 0, 0)
  cairo_surface_t *icon;
#else
  GdkPixmap *icon;
  gint x, y;
#endif
  GtkTreePath *path;

  icon_view = BLXO_ICON_VIEW (widget);

  /* if the user uses a custom DnD impl, we don't set the icon here */
  if (!icon_view->priv->dest_set && !icon_view->priv->source_set)
    return;

  item = blxo_icon_view_get_item_at_coords (icon_view,
                                           icon_view->priv->press_start_x,
                                           icon_view->priv->press_start_y,
                                           TRUE,
                                           NULL);

  _blxo_return_if_fail (item != NULL);

#if !GTK_CHECK_VERSION (3, 0, 0)
  x = icon_view->priv->press_start_x - item->area.x + 1;
  y = icon_view->priv->press_start_y - item->area.y + 1;
#endif

  path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1);
  icon = blxo_icon_view_create_drag_icon (icon_view, path);
  gtk_tree_path_free (path);

#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_drag_set_icon_surface (context, icon);
#else
  gtk_drag_set_icon_pixmap (context,
                            gdk_drawable_get_colormap (icon),
                            icon,
                            NULL,
                            x, y);
#endif

  g_object_unref (icon);
}

static void
blxo_icon_view_drag_end (GtkWidget      *widget,
                        GdkDragContext *context)
{
  /* do nothing */
}

static void
blxo_icon_view_drag_data_get (GtkWidget        *widget,
                             GdkDragContext   *context,
                             GtkSelectionData *selection_data,
                             guint             info,
                             guint             drag_time)
{
  BlxoIconView *icon_view;
  GtkTreeModel *model;
  GtkTreePath *source_row;

  icon_view = BLXO_ICON_VIEW (widget);
  model = blxo_icon_view_get_model (icon_view);

  if (model == NULL)
    return;

  if (!icon_view->priv->dest_set)
    return;

  source_row = get_source_row (context);

  if (source_row == NULL)
    return;

  /* We can implement the GTK_TREE_MODEL_ROW target generically for
   * any model; for DragSource models there are some other targets
   * we also support.
   */

  if (GTK_IS_TREE_DRAG_SOURCE (model) &&
      gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (model),
                                          source_row,
                                          selection_data))
    goto done;

  /* If drag_data_get does nothing, try providing row data. */
  if (gtk_selection_data_get_target (selection_data) == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE))
    gtk_tree_set_row_drag_data (selection_data,
                                model,
                                source_row);

 done:
  gtk_tree_path_free (source_row);
}

static void
blxo_icon_view_drag_data_delete (GtkWidget      *widget,
                                GdkDragContext *context)
{
  GtkTreeModel *model;
  BlxoIconView *icon_view;
  GtkTreePath *source_row;

  icon_view = BLXO_ICON_VIEW (widget);
  model = blxo_icon_view_get_model (icon_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_SOURCE, "drag_data_delete"))
    return;

  if (!icon_view->priv->dest_set)
    return;

  source_row = get_source_row (context);

  if (source_row == NULL)
    return;

  gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (model),
                                         source_row);

  gtk_tree_path_free (source_row);

  set_source_row (context, NULL, NULL);
}

/* Target side drag signals */
static void
blxo_icon_view_drag_leave (GtkWidget      *widget,
                          GdkDragContext *context,
                          guint           drag_time)
{
  BlxoIconView *icon_view;

  icon_view = BLXO_ICON_VIEW (widget);

  /* unset any highlight row */
  blxo_icon_view_set_drag_dest_item (icon_view,
                                    NULL,
                                    BLXO_ICON_VIEW_DROP_LEFT);

  remove_scroll_timeout (icon_view);
}

static gboolean
blxo_icon_view_drag_motion (GtkWidget      *widget,
                           GdkDragContext *context,
                           gint            x,
                           gint            y,
                           guint           drag_time)
{
  BlxoIconViewDropPosition pos;
  GdkDragAction           suggested_action = 0;
  GtkTreePath            *path = NULL;
  BlxoIconView            *icon_view = BLXO_ICON_VIEW (widget);
  gboolean                empty;
  GdkAtom                 target;

  if (!set_destination (icon_view, context, x, y, &suggested_action, &target))
    return FALSE;

  blxo_icon_view_get_drag_dest_item (icon_view, &path, &pos);

  /* we only know this *after* set_desination_row */
  empty = icon_view->priv->empty_view_drop;

  if (path == NULL && !empty)
    {
      /* Can't drop here. */
      gdk_drag_status (context, 0, drag_time);
    }
  else
    {
      if (icon_view->priv->scroll_timeout_id == 0)
        icon_view->priv->scroll_timeout_id = gdk_threads_add_timeout (50, drag_scroll_timeout, icon_view);

      if (target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE))
        {
          /* Request data so we can use the source row when
           * determining whether to accept the drop
           */
          set_status_pending (context, suggested_action);
          gtk_drag_get_data (widget, context, target, drag_time);
        }
      else
        {
          set_status_pending (context, 0);
          gdk_drag_status (context, suggested_action, drag_time);
        }
    }

  if (path != NULL)
    gtk_tree_path_free (path);

  return TRUE;
}

static gboolean
blxo_icon_view_drag_drop (GtkWidget      *widget,
                         GdkDragContext *context,
                         gint            x,
                         gint            y,
                         guint           drag_time)
{
  BlxoIconView *icon_view;
  GtkTreePath *path;
  GdkDragAction suggested_action = 0;
  GdkAtom target = GDK_NONE;
  GtkTreeModel *model;
  gboolean drop_append_mode;

  icon_view = BLXO_ICON_VIEW (widget);
  model = blxo_icon_view_get_model (icon_view);

  remove_scroll_timeout (BLXO_ICON_VIEW (widget));

  if (!icon_view->priv->dest_set)
    return FALSE;

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag_drop"))
    return FALSE;

  if (!set_destination (icon_view, context, x, y, &suggested_action, &target))
    return FALSE;

  path = get_logical_destination (icon_view, &drop_append_mode);

  if (target != GDK_NONE && path != NULL)
    {
      /* in case a motion had requested drag data, change things so we
       * treat drag data receives as a drop.
       */
      set_status_pending (context, 0);
      set_dest_row (context, model, path,
                    icon_view->priv->empty_view_drop, drop_append_mode);
    }

  if (path)
    gtk_tree_path_free (path);

  /* Unset this thing */
  blxo_icon_view_set_drag_dest_item (icon_view, NULL, BLXO_ICON_VIEW_DROP_LEFT);

  if (target != GDK_NONE)
    {
      gtk_drag_get_data (widget, context, target, drag_time);
      return TRUE;
    }
  else
    return FALSE;
}

static void
blxo_icon_view_drag_data_received (GtkWidget        *widget,
                                  GdkDragContext   *context,
                                  gint              x,
                                  gint              y,
                                  GtkSelectionData *selection_data,
                                  guint             info,
                                  guint             drag_time)
{
  GtkTreePath *path;
  gboolean accepted = FALSE;
  GtkTreeModel *model;
  BlxoIconView *icon_view;
  GtkTreePath *dest_row;
  GdkDragAction suggested_action;
  gboolean drop_append_mode;

  icon_view = BLXO_ICON_VIEW (widget);
  model = blxo_icon_view_get_model (icon_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag_data_received"))
    return;

  if (!icon_view->priv->dest_set)
    return;

  suggested_action = get_status_pending (context);

  if (suggested_action)
    {
      /* We are getting this data due to a request in drag_motion,
       * rather than due to a request in drag_drop, so we are just
       * supposed to call drag_status, not actually paste in the
       * data.
       */
      path = get_logical_destination (icon_view, &drop_append_mode);

      if (path == NULL)
        suggested_action = 0;

      if (suggested_action)
        {
          if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
                                                     path,
                                                     selection_data))
            suggested_action = 0;
        }

      gdk_drag_status (context, suggested_action, drag_time);

      if (path)
        gtk_tree_path_free (path);

      /* If you can't drop, remove user drop indicator until the next motion */
      if (suggested_action == 0)
        blxo_icon_view_set_drag_dest_item (icon_view,
                                          NULL,
                                          BLXO_ICON_VIEW_DROP_LEFT);
      return;
    }


  dest_row = get_dest_row (context);

  if (dest_row == NULL)
    return;

  if (gtk_selection_data_get_length (selection_data) >= 0)
    {
      if (gtk_tree_drag_dest_drag_data_received (GTK_TREE_DRAG_DEST (model),
                                                 dest_row,
                                                 selection_data))
        accepted = TRUE;
    }

  gtk_drag_finish (context,
                   accepted,
                   (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE),
                   drag_time);

  gtk_tree_path_free (dest_row);

  /* drop dest_row */
  set_dest_row (context, NULL, NULL, FALSE, FALSE);
}



/**
 * blxo_icon_view_enable_model_drag_source:
 * @icon_view         : a #GtkIconTreeView
 * @start_button_mask : Mask of allowed buttons to start drag
 * @targets           : the table of targets that the drag will support
 * @n_targets         : the number of items in @targets
 * @actions           : the bitmask of possible actions for a drag from this widget
 *
 * Turns @icon_view into a drag source for automatic DND.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_enable_model_drag_source (BlxoIconView              *icon_view,
                                        GdkModifierType           start_button_mask,
                                        const GtkTargetEntry     *targets,
                                        gint                      n_targets,
                                        GdkDragAction             actions)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  gtk_drag_source_set (GTK_WIDGET (icon_view), 0, NULL, 0, actions);

  clear_source_info (icon_view);
  icon_view->priv->start_button_mask = start_button_mask;
  icon_view->priv->source_targets = gtk_target_list_new (targets, n_targets);
  icon_view->priv->source_actions = actions;

  icon_view->priv->source_set = TRUE;

  unset_reorderable (icon_view);
}



/**
 * blxo_icon_view_enable_model_drag_dest:
 * @icon_view : a #BlxoIconView
 * @targets   : the table of targets that the drag will support
 * @n_targets : the number of items in @targets
 * @actions   : the bitmask of possible actions for a drag from this widget
 *
 * Turns @icon_view into a drop destination for automatic DND.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_enable_model_drag_dest (BlxoIconView          *icon_view,
                                      const GtkTargetEntry *targets,
                                      gint                  n_targets,
                                      GdkDragAction         actions)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  gtk_drag_dest_set (GTK_WIDGET (icon_view), 0, NULL, 0, actions);

  clear_dest_info (icon_view);

  icon_view->priv->dest_targets = gtk_target_list_new (targets, n_targets);
  icon_view->priv->dest_actions = actions;

  icon_view->priv->dest_set = TRUE;

  unset_reorderable (icon_view);
}



/**
 * blxo_icon_view_unset_model_drag_source:
 * @icon_view : a #BlxoIconView
 *
 * Undoes the effect of #blxo_icon_view_enable_model_drag_source().
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_unset_model_drag_source (BlxoIconView *icon_view)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->source_set)
    {
      gtk_drag_source_unset (GTK_WIDGET (icon_view));
      clear_source_info (icon_view);
    }

  unset_reorderable (icon_view);
}



/**
 * blxo_icon_view_unset_model_drag_dest:
 * @icon_view : a #BlxoIconView
 *
 * Undoes the effect of #blxo_icon_view_enable_model_drag_dest().
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_unset_model_drag_dest (BlxoIconView *icon_view)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->dest_set)
    {
      gtk_drag_dest_unset (GTK_WIDGET (icon_view));
      clear_dest_info (icon_view);
    }

  unset_reorderable (icon_view);
}



/**
 * blxo_icon_view_set_drag_dest_item:
 * @icon_view : a #BlxoIconView
 * @path      : The path of the item to highlight, or %NULL.
 * @pos       : Specifies whether to drop, relative to the item
 *
 * Sets the item that is highlighted for feedback.
 *
 * Since: 0.3.1
 */
void
blxo_icon_view_set_drag_dest_item (BlxoIconView            *icon_view,
                                  GtkTreePath            *path,
                                  BlxoIconViewDropPosition pos)
{
  BlxoIconViewItem *item;
  GtkTreePath     *previous_path;

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  if (icon_view->priv->dest_item != NULL)
    {
      /* determine and reset the previous path */
      previous_path = gtk_tree_row_reference_get_path (icon_view->priv->dest_item);
      gtk_tree_row_reference_free (icon_view->priv->dest_item);
      icon_view->priv->dest_item = NULL;

      /* check if the path is still valid */
      if (G_LIKELY (previous_path != NULL))
        {
          /* schedule a redraw for the previous path */
          item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices (previous_path)[0]);
          if (G_LIKELY (item != NULL))
            blxo_icon_view_queue_draw_item (icon_view, item);
          gtk_tree_path_free (previous_path);
        }
    }

  /* special case a drop on an empty model */
  icon_view->priv->empty_view_drop = FALSE;
  if (pos == BLXO_ICON_VIEW_NO_DROP
      && path != NULL
      && gtk_tree_path_get_depth (path) == 1
      && gtk_tree_path_get_indices (path)[0] == 0)
    {
      gint n_children;

      n_children = gtk_tree_model_iter_n_children (icon_view->priv->model,
                                                   NULL);

      if (n_children == 0)
        icon_view->priv->empty_view_drop = TRUE;
    }

  icon_view->priv->dest_pos = pos;

  if (G_LIKELY (path != NULL))
    {
      /* take a row reference for the new item path */
      icon_view->priv->dest_item = gtk_tree_row_reference_new_proxy (G_OBJECT (icon_view), icon_view->priv->model, path);

      /* schedule a redraw on the new path */
      item = g_list_nth_data (icon_view->priv->items, gtk_tree_path_get_indices (path)[0]);
      if (G_LIKELY (item != NULL))
        blxo_icon_view_queue_draw_item (icon_view, item);
    }
}



/**
 * blxo_icon_view_get_drag_dest_item:
 * @icon_view : a #BlxoIconView
 * @path      : Return location for the path of the highlighted item, or %NULL.
 * @pos       : Return location for the drop position, or %NULL
 *
 * Gets information about the item that is highlighted for feedback.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_get_drag_dest_item (BlxoIconView              *icon_view,
                                  GtkTreePath             **path,
                                  BlxoIconViewDropPosition  *pos)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (path)
    {
      if (icon_view->priv->dest_item)
        *path = gtk_tree_row_reference_get_path (icon_view->priv->dest_item);
      else
        *path = NULL;
    }

  if (pos)
    *pos = icon_view->priv->dest_pos;
}



/**
 * blxo_icon_view_get_dest_item_at_pos:
 * @icon_view : a #BlxoIconView
 * @drag_x    : the position to determine the destination item for
 * @drag_y    : the position to determine the destination item for
 * @path      : Return location for the path of the highlighted item, or %NULL.
 * @pos       : Return location for the drop position, or %NULL
 *
 * Determines the destination item for a given position.
 *
 * Both @drag_x and @drag_y are given in icon window coordinates. Use
 * #blxo_icon_view_widget_to_icon_coords() if you need to translate
 * widget coordinates first.
 *
 * Returns: whether there is an item at the given position.
 *
 * Since: 0.3.1
 **/
gboolean
blxo_icon_view_get_dest_item_at_pos (BlxoIconView              *icon_view,
                                    gint                      drag_x,
                                    gint                      drag_y,
                                    GtkTreePath             **path,
                                    BlxoIconViewDropPosition  *pos)
{
  BlxoIconViewItem *item;

  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);
  g_return_val_if_fail (drag_x >= 0, FALSE);
  g_return_val_if_fail (drag_y >= 0, FALSE);
  g_return_val_if_fail (icon_view->priv->bin_window != NULL, FALSE);

  if (G_LIKELY (path != NULL))
    *path = NULL;

  item = blxo_icon_view_get_item_at_coords (icon_view, drag_x, drag_y, FALSE, NULL);

  if (G_UNLIKELY (item == NULL))
    return FALSE;

  if (G_LIKELY (path != NULL))
    *path = gtk_tree_path_new_from_indices (g_list_index (icon_view->priv->items, item), -1);

  if (G_LIKELY (pos != NULL))
    {
      if (drag_x < item->area.x + item->area.width / 4)
        *pos = BLXO_ICON_VIEW_DROP_LEFT;
      else if (drag_x > item->area.x + item->area.width * 3 / 4)
        *pos = BLXO_ICON_VIEW_DROP_RIGHT;
      else if (drag_y < item->area.y + item->area.height / 4)
        *pos = BLXO_ICON_VIEW_DROP_ABOVE;
      else if (drag_y > item->area.y + item->area.height * 3 / 4)
        *pos = BLXO_ICON_VIEW_DROP_BELOW;
      else
        *pos = BLXO_ICON_VIEW_DROP_INTO;
    }

  return TRUE;
}



#if GTK_CHECK_VERSION (3, 0, 0)
/**
 * blxo_icon_view_create_drag_icon:
 * @icon_view : a #BlxoIconView
 * @path      : a #GtkTreePath in @icon_view
 *
 * Creates a #cairo_surface_t representation of the item at @path.
 * This image is used for a drag icon.
 *
 * Returns: a newly-allocated pixmap of the drag icon.
 *
 * Since: 0.3.1
 **/
cairo_surface_t*
blxo_icon_view_create_drag_icon (BlxoIconView *icon_view,
                                GtkTreePath *path)
{
  cairo_surface_t *surface;
  cairo_t         *cr;
  GList           *lp;
  gint             idx;
  BlxoIconViewItem *item;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), NULL);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, NULL);

  /* verify that the widget is realized */
  if (G_UNLIKELY (!gtk_widget_get_realized (GTK_WIDGET (icon_view))))
    return NULL;

  idx = gtk_tree_path_get_indices (path)[0];

  for (lp = icon_view->priv->items; lp != NULL; lp = lp->next)
    {
      item = lp->data;
      if (G_UNLIKELY (idx == g_list_index (icon_view->priv->items, item)))
        {
          surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                item->area.width + 2,
                                                item->area.height + 2);

          cr = cairo_create (surface);

          /* TODO: background / rectangles */

          blxo_icon_view_paint_item (icon_view, item, cr, 1, 1, FALSE);

          cairo_destroy (cr);

          return surface;
        }
    }

  return NULL;
}
#else
/**
 * blxo_icon_view_create_drag_icon:
 * @icon_view : a #BlxoIconView
 * @path      : a #GtkTreePath in @icon_view
 *
 * Creates a #GdkPixmap representation of the item at @path.
 * This image is used for a drag icon.
 *
 * Returns: a newly-allocated pixmap of the drag icon.
 *
 * Since: 0.3.1
 **/
GdkPixmap*
blxo_icon_view_create_drag_icon (BlxoIconView *icon_view,
                                GtkTreePath *path)
{
  GdkRectangle     area;
  GtkWidget       *widget = GTK_WIDGET (icon_view);
  GdkPixmap       *drawable;
  GdkGC           *gc;
  GList           *lp;
  gint             idx;
  BlxoIconViewItem *item;

  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), NULL);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, NULL);

  /* verify that the widget is realized */
  if (G_UNLIKELY (!gtk_widget_get_realized (GTK_WIDGET (icon_view))))
    return NULL;

  idx = gtk_tree_path_get_indices (path)[0];

  for (lp = icon_view->priv->items; lp != NULL; lp = lp->next)
    {
      item = lp->data;
      if (G_UNLIKELY (idx == g_list_index (icon_view->priv->items, item)))
        {
          drawable = gdk_pixmap_new (icon_view->priv->bin_window,
                                     item->area.width + 2,
                                     item->area.height + 2,
                                     -1);

          gc = gdk_gc_new (drawable);
          gdk_gc_set_rgb_fg_color (gc, &gtk_widget_get_style (widget)->base[gtk_widget_get_state (widget)]);
          gdk_draw_rectangle (drawable, gc, TRUE, 0, 0, item->area.width + 2, item->area.height + 2);

          area.x = 0;
          area.y = 0;
          area.width = item->area.width;
          area.height = item->area.height;

          blxo_icon_view_paint_item (icon_view, item, &area, drawable, 1, 1, FALSE);

          gdk_gc_set_rgb_fg_color (gc, &gtk_widget_get_style (widget)->black);
          gdk_draw_rectangle (drawable, gc, FALSE, 1, 1, item->area.width + 1, item->area.height + 1);

          g_object_unref (G_OBJECT (gc));

          return drawable;
        }
    }

  return NULL;
}
#endif



/**
 * blxo_icon_view_set_pixbuf_column:
 * @icon_view : a #BlxoIconView
 * @column    : The column that contains the pixbuf to render.
 *
 * Sets the column that contains the pixbuf to render.
 *
 * Since: 0.10.2
 **/
void
blxo_icon_view_set_pixbuf_column (BlxoIconView *icon_view, gint column)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  icon_view->priv->pixbuf_column = column;

  update_pixbuf_cell(icon_view);
}



/**
 * blxo_icon_view_set_icon_column:
 * @icon_view : a #BlxoIconView
 * @column    : The column that contains file to render.
 *
 * Sets the column that contains the file to render.
 *
 * Since: 0.10.2
 **/
void
blxo_icon_view_set_icon_column (BlxoIconView *icon_view, gint column)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  icon_view->priv->icon_column = column;

  update_pixbuf_cell(icon_view);
}



/**
 * blxo_icon_view_get_reorderable:
 * @icon_view : a #BlxoIconView
 *
 * Retrieves whether the user can reorder the list via drag-and-drop.
 * See blxo_icon_view_set_reorderable().
 *
 * Returns: %TRUE if the list can be reordered.
 *
 * Since: 0.3.1
 **/
gboolean
blxo_icon_view_get_reorderable (BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);

  return icon_view->priv->reorderable;
}



/**
 * blxo_icon_view_set_reorderable:
 * @icon_view   : A #BlxoIconView.
 * @reorderable : %TRUE, if the list of items can be reordered.
 *
 * This function is a convenience function to allow you to reorder models that
 * support the #GtkTreeDragSourceIface and the #GtkTreeDragDestIface.  Both
 * #GtkTreeStore and #GtkListStore support these.  If @reorderable is %TRUE, then
 * the user can reorder the model by dragging and dropping rows.  The
 * developer can listen to these changes by connecting to the model's
 * ::row-inserted and ::row-deleted signals.
 *
 * This function does not give you any degree of control over the order -- any
 * reordering is allowed.  If more control is needed, you should probably
 * handle drag and drop manually.
 *
 * Since: 0.3.1
 **/
void
blxo_icon_view_set_reorderable (BlxoIconView *icon_view,
                               gboolean     reorderable)
{
  static const GtkTargetEntry item_targets[] =
  {
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0, },
  };

  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  reorderable = (reorderable != FALSE);

  if (G_UNLIKELY (icon_view->priv->reorderable == reorderable))
    return;

  if (G_LIKELY (reorderable))
    {
      blxo_icon_view_enable_model_drag_source (icon_view, GDK_BUTTON1_MASK, item_targets, G_N_ELEMENTS (item_targets), GDK_ACTION_MOVE);
      blxo_icon_view_enable_model_drag_dest (icon_view, item_targets, G_N_ELEMENTS (item_targets), GDK_ACTION_MOVE);
    }
  else
    {
      blxo_icon_view_unset_model_drag_source (icon_view);
      blxo_icon_view_unset_model_drag_dest (icon_view);
    }

  icon_view->priv->reorderable = reorderable;

  g_object_notify (G_OBJECT (icon_view), "reorderable");
}



/*----------------------*
 * Single-click support *
 *----------------------*/

/**
 * blxo_icon_view_get_single_click:
 * @icon_view : a #BlxoIconView.
 *
 * Returns %TRUE if @icon_view is currently in single click mode,
 * else %FALSE will be returned.
 *
 * Returns: whether @icon_view is currently in single click mode.
 *
 * Since: 0.3.1.3
 **/
gboolean
blxo_icon_view_get_single_click (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);
  return icon_view->priv->single_click;
}



/**
 * blxo_icon_view_set_single_click:
 * @icon_view    : a #BlxoIconView.
 * @single_click : %TRUE for single click, %FALSE for double click mode.
 *
 * If @single_click is %TRUE, @icon_view will be in single click mode
 * afterwards, else @icon_view will be in double click mode.
 *
 * Since: 0.3.1.3
 **/
void
blxo_icon_view_set_single_click (BlxoIconView *icon_view,
                                gboolean     single_click)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  /* normalize the value */
  single_click = !!single_click;

  /* check if we have a new setting here */
  if (icon_view->priv->single_click != single_click)
    {
      icon_view->priv->single_click = single_click;
      g_object_notify (G_OBJECT (icon_view), "single-click");
    }
}



/**
 * blxo_icon_view_get_single_click_timeout:
 * @icon_view : a #BlxoIconView.
 *
 * Returns the amount of time in milliseconds after which the
 * item under the mouse cursor will be selected automatically
 * in single click mode. A value of %0 means that the behavior
 * is disabled and the user must alter the selection manually.
 *
 * Returns: the single click autoselect timeout or %0 if
 *          the behavior is disabled.
 *
 * Since: 0.3.1.5
 **/
guint
blxo_icon_view_get_single_click_timeout (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), 0u);
  return icon_view->priv->single_click_timeout;
}



/**
 * blxo_icon_view_set_single_click_timeout:
 * @icon_view            : a #BlxoIconView.
 * @single_click_timeout : the new timeout or %0 to disable.
 *
 * If @single_click_timeout is a value greater than zero, it specifies
 * the amount of time in milliseconds after which the item under the
 * mouse cursor will be selected automatically in single click mode.
 * A value of %0 for @single_click_timeout disables the autoselection
 * for @icon_view.
 *
 * This setting does not have any effect unless the @icon_view is in
 * single-click mode, see blxo_icon_view_set_single_click().
 *
 * Since: 0.3.1.5
 **/
void
blxo_icon_view_set_single_click_timeout (BlxoIconView *icon_view,
                                        guint        single_click_timeout)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  /* check if we have a new setting */
  if (icon_view->priv->single_click_timeout != single_click_timeout)
    {
      /* apply the new setting */
      icon_view->priv->single_click_timeout = single_click_timeout;

      /* be sure to cancel any pending single click timeout */
      if (G_UNLIKELY (icon_view->priv->single_click_timeout_id != 0))
        g_source_remove (icon_view->priv->single_click_timeout_id);

      /* notify listeners */
      g_object_notify (G_OBJECT (icon_view), "single-click-timeout");
    }
}



static gboolean
blxo_icon_view_single_click_timeout (gpointer user_data)
{
  BlxoIconViewItem *item;
  gboolean         dirty = FALSE;
  BlxoIconView     *icon_view = BLXO_ICON_VIEW (user_data);

  /* verify that we are in single-click mode, have focus and a prelit item */
  if (gtk_widget_has_focus (GTK_WIDGET (icon_view)) && icon_view->priv->single_click && icon_view->priv->prelit_item != NULL)
    {
      /* work on the prelit item */
      item = icon_view->priv->prelit_item;

      /* be sure the item is fully visible */
      blxo_icon_view_scroll_to_item (icon_view, item);

      /* change the selection appropriately */
      if (G_UNLIKELY (icon_view->priv->selection_mode == GTK_SELECTION_NONE))
        {
          blxo_icon_view_set_cursor_item (icon_view, item, -1);
        }
      else if ((icon_view->priv->single_click_timeout_state & GDK_SHIFT_MASK) != 0
            && icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE)
        {
          if (!(icon_view->priv->single_click_timeout_state & GDK_CONTROL_MASK))
            /* unselect all previously selected items */
            blxo_icon_view_unselect_all_internal (icon_view);

          /* select all items between the anchor and the prelit item */
          blxo_icon_view_set_cursor_item (icon_view, item, -1);
          if (icon_view->priv->anchor_item == NULL)
            icon_view->priv->anchor_item = item;
          else
            blxo_icon_view_select_all_between (icon_view, icon_view->priv->anchor_item, item);

          /* selection was changed */
          dirty = TRUE;
        }
      else
        {
          if ((icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE ||
              ((icon_view->priv->selection_mode == GTK_SELECTION_SINGLE) && item->selected)) &&
              (icon_view->priv->single_click_timeout_state & GDK_CONTROL_MASK) != 0)
            {
              item->selected = !item->selected;
              blxo_icon_view_queue_draw_item (icon_view, item);
              dirty = TRUE;
            }
          else if (!item->selected)
            {
              blxo_icon_view_unselect_all_internal (icon_view);
              blxo_icon_view_queue_draw_item (icon_view, item);
              item->selected = TRUE;
              dirty = TRUE;
            }
          blxo_icon_view_set_cursor_item (icon_view, item, -1);
          icon_view->priv->anchor_item = item;
        }
    }

  /* emit "selection-changed" and stop drawing keyboard
   * focus indicator if the selection was altered
   */
  if (G_LIKELY (dirty))
    {
      /* reset "draw keyfocus" flag */
      BLXO_ICON_VIEW_UNSET_FLAG (icon_view, BLXO_ICON_VIEW_DRAW_KEYFOCUS);

      /* emit "selection-changed" */
      g_signal_emit (G_OBJECT (icon_view), icon_view_signals[SELECTION_CHANGED], 0);
    }

  return FALSE;
}



static void
blxo_icon_view_single_click_timeout_destroy (gpointer user_data)
{
  BLXO_ICON_VIEW (user_data)->priv->single_click_timeout_id = 0;
}



/*----------------------------*
 * Interactive search support *
 *----------------------------*/

/**
 * blxo_icon_view_get_enable_search:
 * @icon_view : an #BlxoIconView.
 *
 * Returns whether or not the @icon_view allows to start
 * interactive searching by typing in text.
 *
 * Returns: whether or not to let the user search interactively.
 *
 * Since: 0.3.1.3
 **/
gboolean
blxo_icon_view_get_enable_search (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);
  return icon_view->priv->enable_search;
}



/**
 * blxo_icon_view_set_enable_search:
 * @icon_view     : an #BlxoIconView.
 * @enable_search : %TRUE if the user can search interactively.
 *
 * If @enable_search is set, then the user can type in text to search through
 * the @icon_view interactively (this is sometimes called "typeahead find").
 *
 * Note that even if this is %FALSE, the user can still initiate a search
 * using the "start-interactive-search" key binding.
 *
 * Since: 0.3.1.3
 **/
void
blxo_icon_view_set_enable_search (BlxoIconView *icon_view,
                                 gboolean     enable_search)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  enable_search = !!enable_search;

  if (G_LIKELY (icon_view->priv->enable_search != enable_search))
    {
      icon_view->priv->enable_search = enable_search;
      g_object_notify (G_OBJECT (icon_view), "enable-search");
    }
}



/**
 * blxo_icon_view_get_search_column:
 * @icon_view : an #BlxoIconView.
 *
 * Returns the column searched on by the interactive search code.
 *
 * Returns: the column the interactive search code searches in.
 *
 * Since: 0.3.1.3
 **/
gint
blxo_icon_view_get_search_column (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), -1);
  return icon_view->priv->search_column;
}



/**
 * blxo_icon_view_set_search_column:
 * @icon_view     : an #BlxoIconView.
 * @search_column : the column of the model to search in, or -1 to disable searching.
 *
 * Sets @search_column as the column where the interactive search code should search in.
 *
 * If the search column is set, user can use the "start-interactive-search" key
 * binding to bring up search popup. The "enable-search" property controls
 * whether simply typing text will also start an interactive search.
 *
 * Note that @search_column refers to a column of the model.
 *
 * Since: 0.3.1.3
 **/
void
blxo_icon_view_set_search_column (BlxoIconView *icon_view,
                                 gint         search_column)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (search_column >= -1);

  if (G_LIKELY (icon_view->priv->search_column != search_column))
    {
      icon_view->priv->search_column = search_column;
      g_object_notify (G_OBJECT (icon_view), "search-column");
    }
}



/**
 * blxo_icon_view_get_search_equal_func:
 * @icon_view : an #BlxoIconView.
 *
 * Returns the compare function currently in use.
 *
 * Returns: the currently used compare function for the search code.
 *
 * Since: 0.3.1.3
 **/
BlxoIconViewSearchEqualFunc
blxo_icon_view_get_search_equal_func (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), NULL);
  return icon_view->priv->search_equal_func;
}



/**
 * blxo_icon_view_set_search_equal_func:
 * @icon_view            : an #BlxoIconView.
 * @search_equal_func    : the compare function to use during the search, or %NULL.
 * @search_equal_data    : user data to pass to @search_equal_func, or %NULL.
 * @search_equal_destroy : destroy notifier for @search_equal_data, or %NULL.
 *
 * Sets the compare function for the interactive search capabilities;
 * note that some like strcmp() returning 0 for equality
 * #BlxoIconViewSearchEqualFunc returns %FALSE on matches.
 *
 * Specifying %NULL for @search_equal_func will reset @icon_view to use the default
 * search equal function.
 *
 * Since: 0.3.1.3
 **/
void
blxo_icon_view_set_search_equal_func (BlxoIconView               *icon_view,
                                     BlxoIconViewSearchEqualFunc search_equal_func,
                                     gpointer                   search_equal_data,
                                     GDestroyNotify             search_equal_destroy)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (search_equal_func != NULL || (search_equal_data == NULL && search_equal_destroy == NULL));

  /* destroy the previous data (if any) */
  if (G_UNLIKELY (icon_view->priv->search_equal_destroy != NULL))
    (*icon_view->priv->search_equal_destroy) (icon_view->priv->search_equal_data);

  icon_view->priv->search_equal_func = (search_equal_func != NULL) ? search_equal_func : blxo_icon_view_search_equal_func;
  icon_view->priv->search_equal_data = search_equal_data;
  icon_view->priv->search_equal_destroy = search_equal_destroy;
}



/**
 * blxo_icon_view_get_search_position_func:
 * @icon_view : an #BlxoIconView.
 *
 * Returns the search dialog positioning function currently in use.
 *
 * Returns: the currently used function for positioning the search dialog.
 *
 * Since: 0.3.1.3
 **/
BlxoIconViewSearchPositionFunc
blxo_icon_view_get_search_position_func (const BlxoIconView *icon_view)
{
  g_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), NULL);
  return icon_view->priv->search_position_func;
}



/**
 * blxo_icon_view_set_search_position_func:
 * @icon_view               : an #BlxoIconView.
 * @search_position_func    : the function to use to position the search dialog, or %NULL.
 * @search_position_data    : user data to pass to @search_position_func, or %NULL.
 * @search_position_destroy : destroy notifier for @search_position_data, or %NULL.
 *
 * Sets the function to use when positioning the seach dialog.
 *
 * Specifying %NULL for @search_position_func will reset @icon_view to use the default
 * search position function.
 *
 * Since: 0.3.1.3
 **/
void
blxo_icon_view_set_search_position_func (BlxoIconView                  *icon_view,
                                        BlxoIconViewSearchPositionFunc search_position_func,
                                        gpointer                      search_position_data,
                                        GDestroyNotify                search_position_destroy)
{
  g_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));
  g_return_if_fail (search_position_func != NULL || (search_position_data == NULL && search_position_destroy == NULL));

  /* destroy the previous data (if any) */
  if (icon_view->priv->search_position_destroy != NULL)
    (*icon_view->priv->search_position_destroy) (icon_view->priv->search_position_data);

  icon_view->priv->search_position_func = (search_position_func != NULL) ? search_position_func : blxo_icon_view_search_position_func;
  icon_view->priv->search_position_data = search_position_data;
  icon_view->priv->search_position_destroy = search_position_destroy;
}



static void
blxo_icon_view_search_activate (GtkEntry    *entry,
                               BlxoIconView *icon_view)
{
  GtkTreePath *path;

  /* hide the interactive search dialog */
  blxo_icon_view_search_dialog_hide (icon_view->priv->search_window, icon_view);

  /* check if we have a cursor item, and if so, activate it */
  if (blxo_icon_view_get_cursor (icon_view, &path, NULL))
    {
      /* only activate the cursor item if it's selected */
      if (blxo_icon_view_path_is_selected (icon_view, path))
        blxo_icon_view_item_activated (icon_view, path);
      gtk_tree_path_free (path);
    }
}



static void
blxo_icon_view_search_dialog_hide (GtkWidget   *search_dialog,
                                  BlxoIconView *icon_view)
{
  _blxo_return_if_fail (GTK_IS_WIDGET (search_dialog));
  _blxo_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  if (icon_view->priv->search_disable_popdown)
    return;

  /* disconnect the "changed" signal handler */
  if (icon_view->priv->search_entry_changed_id != 0)
    {
      g_signal_handler_disconnect (G_OBJECT (icon_view->priv->search_entry), icon_view->priv->search_entry_changed_id);
      icon_view->priv->search_entry_changed_id = 0;
    }

  /* disable the flush timeout */
  if (icon_view->priv->search_timeout_id != 0)
    g_source_remove (icon_view->priv->search_timeout_id);

  /* send focus-out event */
  _blxo_gtk_widget_send_focus_change (icon_view->priv->search_entry, FALSE);
  gtk_widget_hide (search_dialog);
  gtk_entry_set_text (GTK_ENTRY (icon_view->priv->search_entry), "");
}



static void
blxo_icon_view_search_ensure_directory (BlxoIconView *icon_view)
{
  GtkWidget *toplevel;
  GtkWidget *frame;
  GtkWidget *vbox;

  /* determine the toplevel window */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (icon_view));

  /* check if we already have a search window */
  if (G_LIKELY (icon_view->priv->search_window != NULL))
    {
      if (gtk_window_get_group (GTK_WINDOW (toplevel)) != NULL)
        gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)), GTK_WINDOW (icon_view->priv->search_window));
      else if (gtk_window_get_group (GTK_WINDOW (icon_view->priv->search_window)) != NULL)
        gtk_window_group_remove_window (gtk_window_get_group (GTK_WINDOW (icon_view->priv->search_window)), GTK_WINDOW (icon_view->priv->search_window));
      return;
    }

  /* allocate a new search window */
  icon_view->priv->search_window = gtk_window_new (GTK_WINDOW_POPUP);
  if (gtk_window_get_group (GTK_WINDOW (toplevel)) != NULL)
    gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)), GTK_WINDOW (icon_view->priv->search_window));
  gtk_window_set_modal (GTK_WINDOW (icon_view->priv->search_window), TRUE);
  gtk_window_set_screen (GTK_WINDOW (icon_view->priv->search_window), gtk_widget_get_screen (GTK_WIDGET (icon_view)));

  /* connect signal handlers */
  g_signal_connect (G_OBJECT (icon_view->priv->search_window), "delete-event", G_CALLBACK (blxo_icon_view_search_delete_event), icon_view);
  g_signal_connect (G_OBJECT (icon_view->priv->search_window), "scroll-event", G_CALLBACK (blxo_icon_view_search_scroll_event), icon_view);
  g_signal_connect (G_OBJECT (icon_view->priv->search_window), "key-press-event", G_CALLBACK (blxo_icon_view_search_key_press_event), icon_view);
  g_signal_connect (G_OBJECT (icon_view->priv->search_window), "button-press-event", G_CALLBACK (blxo_icon_view_search_button_press_event), icon_view);

  /* allocate the frame widget */
  frame = g_object_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_ETCHED_IN, NULL);
  gtk_container_add (GTK_CONTAINER (icon_view->priv->search_window), frame);
  gtk_widget_show (frame);

  /* allocate the vertical box */
#if GTK_CHECK_VERSION (3, 0, 0)
  vbox = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "border-width", 3, NULL);
#else
  vbox = g_object_new (GTK_TYPE_VBOX, "border-width", 3, NULL);
#endif
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  /* allocate the search entry widget */
  icon_view->priv->search_entry = gtk_entry_new ();
#if GTK_CHECK_VERSION(3, 22, 20)
  gtk_entry_set_input_hints (GTK_ENTRY (icon_view->priv->search_entry), GTK_INPUT_HINT_NO_EMOJI);
#endif
  g_signal_connect (G_OBJECT (icon_view->priv->search_entry), "activate", G_CALLBACK (blxo_icon_view_search_activate), icon_view);
  gtk_box_pack_start (GTK_BOX (vbox), icon_view->priv->search_entry, TRUE, TRUE, 0);
  gtk_widget_realize (icon_view->priv->search_entry);
  gtk_widget_show (icon_view->priv->search_entry);
}



static void
blxo_icon_view_search_init (GtkWidget   *search_entry,
                           BlxoIconView *icon_view)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  const gchar  *text;
  gint          length;
  gint          count = 0;

  _blxo_return_if_fail (GTK_IS_ENTRY (search_entry));
  _blxo_return_if_fail (BLXO_IS_ICON_VIEW (icon_view));

  /* determine the current text for the search entry */
  text = gtk_entry_get_text (GTK_ENTRY (search_entry));
  if (G_UNLIKELY (text == NULL))
    return;

  /* unselect all items */
  blxo_icon_view_unselect_all (icon_view);

  /* renew the flush timeout */
  if ((icon_view->priv->search_timeout_id != 0))
    {
      /* drop the previous timeout */
      g_source_remove (icon_view->priv->search_timeout_id);

      /* schedule a new timeout */
      icon_view->priv->search_timeout_id = gdk_threads_add_timeout_full (G_PRIORITY_LOW, BLXO_ICON_VIEW_SEARCH_DIALOG_TIMEOUT,
                                                               blxo_icon_view_search_timeout, icon_view,
                                                               blxo_icon_view_search_timeout_destroy);
    }

  /* verify that we have a search text */
  length = strlen (text);
  if (length < 1)
    return;

  /* verify that we have a valid model */
  model = blxo_icon_view_get_model (icon_view);
  if (G_UNLIKELY (model == NULL))
    return;

  /* start the interactive search */
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      /* let's see if we have a match */
      if (blxo_icon_view_search_iter (icon_view, model, &iter, text, &count, 1))
        icon_view->priv->search_selected_iter = 1;
    }
}



static gboolean
blxo_icon_view_search_iter (BlxoIconView  *icon_view,
                           GtkTreeModel *model,
                           GtkTreeIter  *iter,
                           const gchar  *text,
                           gint         *count,
                           gint          n)
{
  GtkTreePath *path;

  _blxo_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);
  _blxo_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);
  _blxo_return_val_if_fail (count != NULL, FALSE);

  /* search for a matching item */
  do
    {
      if (!(*icon_view->priv->search_equal_func) (model, icon_view->priv->search_column, text, iter, icon_view->priv->search_equal_data))
        {
          (*count) += 1;
          if (*count == n)
            {
              /* place cursor on the item and select it */
              path = gtk_tree_model_get_path (model, iter);
              blxo_icon_view_select_path (icon_view, path);
              blxo_icon_view_set_cursor (icon_view, path, NULL, FALSE);
              gtk_tree_path_free (path);
              return TRUE;
            }
        }
    }
  while (gtk_tree_model_iter_next (model, iter));

  /* no match */
  return FALSE;
}



static void
blxo_icon_view_search_move (GtkWidget   *widget,
                           BlxoIconView *icon_view,
                           gboolean     move_up)
{
  GtkTreeModel *model;
  const gchar  *text;
  GtkTreeIter   iter;
  gboolean      retval;
  gint          length;
  gint          count = 0;

  /* determine the current text for the search entry */
  text = gtk_entry_get_text (GTK_ENTRY (icon_view->priv->search_entry));
  if (G_UNLIKELY (text == NULL))
    return;

  /* if we already selected the first item, we cannot go up */
  if (move_up && icon_view->priv->search_selected_iter == 1)
    return;

  /* determine the length of the search text */
  length = strlen (text);
  if (G_UNLIKELY (length < 1))
    return;

  /* unselect all items */
  blxo_icon_view_unselect_all (icon_view);

  /* verify that we have a valid model */
  model = blxo_icon_view_get_model (icon_view);
  if (G_UNLIKELY (model == NULL))
    return;

  /* determine the iterator to the first item */
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  /* first attempt to search */
  retval = blxo_icon_view_search_iter (icon_view, model, &iter, text, &count, move_up
                                      ? (icon_view->priv->search_selected_iter - 1)
                                      : (icon_view->priv->search_selected_iter + 1));

  /* check if we found something */
  if (G_LIKELY (retval))
    {
      /* match found */
      icon_view->priv->search_selected_iter += move_up ? -1 : 1;
    }
  else
    {
      /* return to old iter */
      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          count = 0;
          blxo_icon_view_search_iter (icon_view, model, &iter, text, &count,
                                     icon_view->priv->search_selected_iter);
        }
    }
}



static gboolean
blxo_icon_view_search_start (BlxoIconView *icon_view,
                            gboolean     keybinding)
{
#if !GTK_CHECK_VERSION (3,16,0)
  GTypeClass *klass;
#endif

  /* check if typeahead is enabled */
  if (G_UNLIKELY (!icon_view->priv->enable_search && !keybinding))
    return FALSE;

  /* check if we already display the search window */
  if (icon_view->priv->search_window != NULL && gtk_widget_get_visible (icon_view->priv->search_window))
    return TRUE;

  /* we only start interactive search if we have focus,
   * we don't want to start interactive search if one of
   * our children has the focus.
   */
  if (!gtk_widget_has_focus (GTK_WIDGET (icon_view)))
    return FALSE;

  /* verify that we have a search column */
  if (G_UNLIKELY (icon_view->priv->search_column < 0))
    return FALSE;

  blxo_icon_view_search_ensure_directory (icon_view);

  /* clear search entry if we were started by a keybinding */
  if (G_UNLIKELY (keybinding))
    gtk_entry_set_text (GTK_ENTRY (icon_view->priv->search_entry), "");

  /* determine the position for the search dialog */
  (*icon_view->priv->search_position_func) (icon_view, icon_view->priv->search_window, icon_view->priv->search_position_data);

#if GTK_CHECK_VERSION (3,16,0)
  gtk_entry_grab_focus_without_selecting (GTK_ENTRY (icon_view->priv->search_entry));
#else
  /* grab focus will select all the text, we don't want that to happen, so we
   * call the parent instance and bypass the selection change. This is probably
   * really hackish, but GtkTreeView does it as well *hrhr*
   */
  klass = g_type_class_peek_parent (GTK_ENTRY_GET_CLASS (icon_view->priv->search_entry));
  (*GTK_WIDGET_CLASS (klass)->grab_focus) (icon_view->priv->search_entry);
#endif

  /* display the search dialog */
  gtk_widget_show (icon_view->priv->search_window);

  /* connect "changed" signal for the entry */
  if (G_UNLIKELY (icon_view->priv->search_entry_changed_id == 0))
    {
      icon_view->priv->search_entry_changed_id = g_signal_connect (G_OBJECT (icon_view->priv->search_entry), "changed",
                                                                   G_CALLBACK (blxo_icon_view_search_init), icon_view);
    }

  /* start the search timeout */
  icon_view->priv->search_timeout_id = gdk_threads_add_timeout_full (G_PRIORITY_LOW, BLXO_ICON_VIEW_SEARCH_DIALOG_TIMEOUT,
                                                           blxo_icon_view_search_timeout, icon_view,
                                                           blxo_icon_view_search_timeout_destroy);

  /* send focus-in event */
  _blxo_gtk_widget_send_focus_change (icon_view->priv->search_entry, TRUE);

  /* search first matching iter */
  blxo_icon_view_search_init (icon_view->priv->search_entry, icon_view);

  return TRUE;
}



static gboolean
blxo_icon_view_search_equal_func (GtkTreeModel *model,
                                 gint          column,
                                 const gchar  *key,
                                 GtkTreeIter  *iter,
                                 gpointer      user_data)
{
  const gchar *str;
  gboolean     retval = TRUE;
  GValue       transformed = { 0, };
  GValue       value = { 0, };
  gchar       *case_normalized_string = NULL;
  gchar       *case_normalized_key = NULL;
  gchar       *normalized_string;
  gchar       *normalized_key;

  /* determine the value for the column/iter */
  gtk_tree_model_get_value (model, iter, column, &value);

  /* try to transform the value to a string */
  g_value_init (&transformed, G_TYPE_STRING);
  if (!g_value_transform (&value, &transformed))
    {
      g_value_unset (&value);
      return TRUE;
    }
  g_value_unset (&value);

  /* check if we have a string value */
  str = g_value_get_string (&transformed);
  if (G_UNLIKELY (str == NULL))
    {
      g_value_unset (&transformed);
      return TRUE;
    }

  /* normalize the string and the key */
  normalized_string = g_utf8_normalize (str, -1, G_NORMALIZE_ALL);
  normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);

  /* check if we have normalized both string */
  if (G_LIKELY (normalized_string != NULL && normalized_key != NULL))
    {
      case_normalized_string = g_utf8_casefold (normalized_string, -1);
      case_normalized_key = g_utf8_casefold (normalized_key, -1);

      /* compare the casefolded strings */
      if (strncmp (case_normalized_key, case_normalized_string, strlen (case_normalized_key)) == 0)
        retval = FALSE;
    }

  /* cleanup */
  g_free (case_normalized_string);
  g_free (case_normalized_key);
  g_value_unset (&transformed);
  g_free (normalized_string);
  g_free (normalized_key);

  return retval;
}



static void
blxo_icon_view_search_position_func (BlxoIconView *icon_view,
                                    GtkWidget   *search_dialog,
                                    gpointer     user_data)
{
  GtkRequisition requisition;
  GdkWindow     *view_window = gtk_widget_get_window (GTK_WIDGET (icon_view));
  GdkRectangle   work_area_dimensions;
  gint           view_width, view_height;
  gint           view_x, view_y;
  gint           x, y;
  GdkDisplay    *display;
  GdkRectangle   monitor_dimensions;
#if GTK_CHECK_VERSION (3, 22, 0)
  GdkMonitor    *monitor;
#else
  GdkScreen     *screen;
  gint           monitor_num;
#endif

  /* make sure the search dialog is realized */
  gtk_widget_realize (search_dialog);

  gdk_window_get_origin (view_window, &view_x, &view_y);
  view_width = gdk_window_get_width (view_window);
  view_height = gdk_window_get_height (view_window);

#if GTK_CHECK_VERSION (3, 0, 0)
  /* FIXME: make actual use of new Gtk3 layout system */
  gtk_widget_get_preferred_width (search_dialog, NULL, &requisition.width);
  gtk_widget_get_preferred_height (search_dialog, NULL, &requisition.height);
#else
  gtk_widget_size_request (search_dialog, &requisition);
#endif

  blxo_icon_view_get_work_area_dimensions (view_window, &work_area_dimensions);
  if (view_x + view_width > work_area_dimensions.x + work_area_dimensions.width)
    x = work_area_dimensions.x + work_area_dimensions.width - requisition.width;
  else if (view_x + view_width - requisition.width < work_area_dimensions.x)
    x = work_area_dimensions.x;
  else
    x = view_x + view_width - requisition.width;

  if (view_y + view_height > work_area_dimensions.y + work_area_dimensions.height)
    y = work_area_dimensions.y + work_area_dimensions.height - requisition.height;
  else if (view_y + view_height < work_area_dimensions.y)
    y = work_area_dimensions.y;
  else
    y = view_y + view_height;

  display = gdk_window_get_display (view_window);
  if (display)
    {
#if GTK_CHECK_VERSION (3, 22, 0)
      monitor = gdk_display_get_monitor_at_window (display, view_window);
      if (monitor)
        {
          gdk_monitor_get_geometry (monitor, &monitor_dimensions);
          if (y + requisition.height > monitor_dimensions.height)
            y = monitor_dimensions.height - requisition.height;
        }
#else
      screen = gdk_display_get_default_screen (display);
      monitor_num = gdk_screen_get_monitor_at_window (screen, view_window);
      if (monitor_num >= 0)
        {
          gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor_dimensions);
          if (y + requisition.height > monitor_dimensions.height)
            y = monitor_dimensions.height - requisition.height;
        }
#endif
    }

  gtk_window_move (GTK_WINDOW (search_dialog), x, y);
}



static gboolean
blxo_icon_view_search_button_press_event (GtkWidget      *widget,
                                         GdkEventButton *event,
                                         BlxoIconView    *icon_view)
{
  _blxo_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  _blxo_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);

  /* hide the search dialog */
  blxo_icon_view_search_dialog_hide (widget, icon_view);

  if (event->window == icon_view->priv->bin_window)
    blxo_icon_view_button_press_event (GTK_WIDGET (icon_view), event);

  return TRUE;
}



static gboolean
blxo_icon_view_search_delete_event (GtkWidget   *widget,
                                   GdkEventAny *event,
                                   BlxoIconView *icon_view)
{
  _blxo_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  _blxo_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);

  /* hide the search dialog */
  blxo_icon_view_search_dialog_hide (widget, icon_view);

  return TRUE;
}



static gboolean
blxo_icon_view_search_key_press_event (GtkWidget   *widget,
                                      GdkEventKey *event,
                                      BlxoIconView *icon_view)
{
  gboolean retval = FALSE;

  _blxo_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  _blxo_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);


  /* close window and cancel the search */
  if (event->keyval == GDK_KEY_Escape || event->keyval == GDK_KEY_Tab)
    {
      blxo_icon_view_search_dialog_hide (widget, icon_view);
      return TRUE;
    }

  /* select previous matching iter */
  if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up)
    {
      blxo_icon_view_search_move (widget, icon_view, TRUE);
      retval = TRUE;
    }

  if (((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
      && (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G))
    {
      blxo_icon_view_search_move (widget, icon_view, TRUE);
      retval = TRUE;
    }

  /* select next matching iter */
  if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down)
    {
      blxo_icon_view_search_move (widget, icon_view, FALSE);
      retval = TRUE;
    }

  if (((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK)
      && (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G))
    {
      blxo_icon_view_search_move (widget, icon_view, FALSE);
      retval = TRUE;
    }

  /* renew the flush timeout */
  if (retval && (icon_view->priv->search_timeout_id != 0))
    {
      /* drop the previous timeout */
      g_source_remove (icon_view->priv->search_timeout_id);

      /* schedule a new timeout */
      icon_view->priv->search_timeout_id = gdk_threads_add_timeout_full (G_PRIORITY_LOW, BLXO_ICON_VIEW_SEARCH_DIALOG_TIMEOUT,
                                                               blxo_icon_view_search_timeout, icon_view,
                                                               blxo_icon_view_search_timeout_destroy);
    }

  return retval;
}



static gboolean
blxo_icon_view_search_scroll_event (GtkWidget      *widget,
                                   GdkEventScroll *event,
                                   BlxoIconView    *icon_view)
{
  gboolean retval = TRUE;

  _blxo_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  _blxo_return_val_if_fail (BLXO_IS_ICON_VIEW (icon_view), FALSE);

  if (event->direction == GDK_SCROLL_UP)
    blxo_icon_view_search_move (widget, icon_view, TRUE);
  else if (event->direction == GDK_SCROLL_DOWN)
    blxo_icon_view_search_move (widget, icon_view, FALSE);
  else
    retval = FALSE;

  return retval;
}



static gboolean
blxo_icon_view_search_timeout (gpointer user_data)
{
  BlxoIconView *icon_view = BLXO_ICON_VIEW (user_data);

  blxo_icon_view_search_dialog_hide (icon_view->priv->search_window, icon_view);

  return FALSE;
}



static void
blxo_icon_view_search_timeout_destroy (gpointer user_data)
{
  BLXO_ICON_VIEW (user_data)->priv->search_timeout_id = 0;
}



#define __BLXO_ICON_VIEW_C__
#include <blxo/blxo-aliasdef.c>
