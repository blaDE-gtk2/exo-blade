/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <gio/gio.h>

#include <blxo/blxo-cell-renderer-icon.h>
#include <blxo/blxo-gdk-pixbuf-extensions.h>
#include <blxo/blxo-private.h>
#include <blxo/blxo-thumbnail.h>
#include <blxo/blxo-alias.h>

/**
 * SECTION: blxo-cell-renderer-icon
 * @title: BlxoCellRendererIcon
 * @short_description: Renders an icon in a cell
 * @include: blxo/blxo.h
 * @see_also: <link linkend="BlxoIconView">BlxoIconView</link>
 *
 * An #BlxoCellRendererIcon can be used to render an icon in a cell. It
 * allows to render either a named icon, which is looked up using the
 * #GtkIconTheme, or an image file loaded from the file system. The icon
 * name or absolute path to the image file is set via the
 * <link linkend="BlxoCellRendererIcon--icon">icon</link> property.
 *
 * To support the <link linkend="BlxoIconView">BlxoIconView</link> (and <link
 * linkend="GtkIconView">GtkIconView</link>), #BlxoCellRendererIcon supports
 * rendering icons based on the state of the view if the
 * <link linkend="BlxoCellRendererIcon--follow-state">follow-state</link>
 * property is set.
 **/

/* HACK: fix dead API via #define */
#if GTK_CHECK_VERSION (3, 0, 0)
# define gtk_icon_info_free(info) g_object_unref (info)
#endif

/* Property identifiers */
enum
{
  PROP_0,
  PROP_FOLLOW_STATE,
  PROP_ICON,
  PROP_GICON,
  PROP_SIZE,
};



static void blxo_cell_renderer_icon_finalize     (GObject                  *object);
static void blxo_cell_renderer_icon_get_property (GObject                  *object,
                                                 guint                     prop_id,
                                                 GValue                   *value,
                                                 GParamSpec               *pspec);
static void blxo_cell_renderer_icon_set_property (GObject                  *object,
                                                 guint                     prop_id,
                                                 const GValue             *value,
                                                 GParamSpec               *pspec);
static void blxo_cell_renderer_icon_get_size     (GtkCellRenderer          *renderer,
                                                 GtkWidget                *widget,
#if GTK_CHECK_VERSION (3, 0, 0)
                                                 const GdkRectangle       *cell_area,
#else
                                                 GdkRectangle             *cell_area,
#endif
                                                 gint                     *x_offset,
                                                 gint                     *y_offset,
                                                 gint                     *width,
                                                 gint                     *height);
#if GTK_CHECK_VERSION (3, 0, 0)
static void blxo_cell_renderer_icon_render       (GtkCellRenderer          *renderer,
                                                 cairo_t                  *cr,
                                                 GtkWidget                *widget,
                                                 const GdkRectangle       *background_area,
                                                 const GdkRectangle       *cell_area,
                                                 GtkCellRendererState      flags);
#else
static void blxo_cell_renderer_icon_render       (GtkCellRenderer          *renderer,
                                                 GdkWindow                *window,
                                                 GtkWidget                *widget,
                                                 GdkRectangle             *background_area,
                                                 GdkRectangle             *cell_area,
                                                 GdkRectangle             *expose_area,
                                                 GtkCellRendererState      flags);
#endif



struct _BlxoCellRendererIconPrivate
{
  guint  follow_state : 1;
  guint  icon_static : 1;
  gchar *icon;
  GIcon *gicon;
  gint   size;
};



G_DEFINE_TYPE_WITH_PRIVATE (BlxoCellRendererIcon, blxo_cell_renderer_icon, GTK_TYPE_CELL_RENDERER)



static void
blxo_cell_renderer_icon_class_init (BlxoCellRendererIconClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = blxo_cell_renderer_icon_finalize;
  gobject_class->get_property = blxo_cell_renderer_icon_get_property;
  gobject_class->set_property = blxo_cell_renderer_icon_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->get_size = blxo_cell_renderer_icon_get_size;
  gtkcell_renderer_class->render = blxo_cell_renderer_icon_render;

  /* initialize the library's i18n support */
  _blxo_i18n_init ();

  /**
   * BlxoCellRendererIcon:follow-state:
   *
   * Specifies whether the icon renderer should render icon based on the
   * selection state of the items. This is necessary for #BlxoIconView,
   * which doesn't draw any item state indicators itself.
   *
   * Since: 0.3.1.9
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLLOW_STATE,
                                   g_param_spec_boolean ("follow-state",
                                                         _("Follow state"),
                                                         _("Render differently based on the selection state."),
                                                         TRUE,
                                                         BLXO_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * BlxoCellRendererIcon:icon:
   *
   * The name of the themed icon to render or an absolute path to an image file
   * to render. May also be %NULL in which case no icon will be rendered for the
   * cell.
   *
   * Image files are loaded via the thumbnail database, creating a thumbnail
   * as necessary. The thumbnail database is also used to load scalable icons
   * in the icon theme, because loading scalable icons is quite expensive
   * these days.
   *
   * Since: 0.3.1.9
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_string ("icon",
                                                        _("Icon"),
                                                        _("The icon to render."),
                                                        NULL,
                                                        BLXO_PARAM_READWRITE));

  /**
   * BlxoCellRendererIcon:gicon:
   *
   * The #GIcon to render. May also be %NULL in which case no icon will be
   * rendered for the cell.
   *
   * Currently only #GThemedIcon<!---->s are supported which are loaded
   * using the current icon theme.
   *
   * Since: 0.4.0
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_GICON,
                                   g_param_spec_object ("gicon",
                                                        _("GIcon"),
                                                        _("The GIcon to render."),
                                                        G_TYPE_ICON,
                                                        BLXO_PARAM_READWRITE));

  /**
   * BlxoCellRendererIcon:size:
   *
   * The size in pixel at which to render the icon. This is also the fixed
   * size that the renderer will request no matter if the actual icons are
   * smaller than this size.
   *
   * This improves the performance of the layouting in the icon and tree
   * view, because during the layouting phase no icons will need to be
   * loaded, but the icons will only be loaded when they need to be rendered,
   * i.e. the view scrolls to the cell.
   *
   * Since: 0.3.1.9
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     _("size"),
                                                     _("The size of the icon to render in pixels."),
                                                     1, G_MAXINT, 48,
                                                     BLXO_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}



static void
blxo_cell_renderer_icon_init (BlxoCellRendererIcon *icon_undocked)
{
}



static void
blxo_cell_renderer_icon_finalize (GObject *object)
{
  const BlxoCellRendererIconPrivate *priv = blxo_cell_renderer_icon_get_instance_private (BLXO_CELL_RENDERER_ICON (object));

  /* free the icon if not static */
  if (!priv->icon_static)
    g_free (priv->icon);

  /* free the GICon */
  if (priv->gicon != NULL)
    g_object_unref (priv->gicon);

  (*G_OBJECT_CLASS (blxo_cell_renderer_icon_parent_class)->finalize) (object);
}



static void
blxo_cell_renderer_icon_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  const BlxoCellRendererIconPrivate *priv = blxo_cell_renderer_icon_get_instance_private (BLXO_CELL_RENDERER_ICON (object));

  switch (prop_id)
    {
    case PROP_FOLLOW_STATE:
      g_value_set_boolean (value, priv->follow_state);
      break;

    case PROP_ICON:
      g_value_set_string (value, priv->icon);
      break;

    case PROP_GICON:
      g_value_set_object (value, priv->gicon);
      break;

    case PROP_SIZE:
      g_value_set_int (value, priv->size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
blxo_cell_renderer_icon_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  BlxoCellRendererIconPrivate *priv = blxo_cell_renderer_icon_get_instance_private (BLXO_CELL_RENDERER_ICON (object));
  const gchar                *icon;

  switch (prop_id)
    {
    case PROP_FOLLOW_STATE:
      priv->follow_state = g_value_get_boolean (value);
      break;

    case PROP_ICON:
      /* release the previous icon (if not static) */
      if (!priv->icon_static)
        g_free (priv->icon);
      icon = g_value_get_string (value);
      priv->icon_static = (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS) ? TRUE : FALSE;
      priv->icon = (gchar *) ((icon == NULL) ? "" : icon);
      if (!priv->icon_static)
        priv->icon = g_strdup (priv->icon);
      break;

    case PROP_GICON:
      if (priv->gicon != NULL)
        g_object_unref (priv->gicon);
      priv->gicon = g_value_dup_object (value);
      break;

    case PROP_SIZE:
      priv->size = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
blxo_cell_renderer_icon_get_size (GtkCellRenderer *renderer,
                                 GtkWidget       *widget,
#if GTK_CHECK_VERSION (3, 0, 0)
                                 const GdkRectangle *cell_area,
#else
                                 GdkRectangle    *cell_area,
#endif
                                 gint            *x_offset,
                                 gint            *y_offset,
                                 gint            *width,
                                 gint            *height)
{
  const BlxoCellRendererIconPrivate *priv = blxo_cell_renderer_icon_get_instance_private (BLXO_CELL_RENDERER_ICON (renderer));
  gfloat xalign, yalign;
  gint   xpad, ypad;

  gtk_cell_renderer_get_alignment (renderer, &xalign, &yalign);
  gtk_cell_renderer_get_padding (renderer, &xpad, &ypad);

  if (cell_area != NULL)
    {
      if (x_offset != NULL)
        {
          *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? 1.0 - xalign : xalign)
                    * (cell_area->width - priv->size);
          *x_offset = MAX (*x_offset, 0) + xpad;
        }

      if (y_offset != NULL)
        {
          *y_offset = yalign * (cell_area->height - priv->size);
          *y_offset = MAX (*y_offset, 0) + ypad;
        }
    }
  else
    {
      if (x_offset != NULL)
        *x_offset = 0;

      if (y_offset != NULL)
        *y_offset = 0;
    }

  if (G_LIKELY (width != NULL))
    *width = (gint) xpad * 2 + priv->size;

  if (G_LIKELY (height != NULL))
    *height = (gint) ypad * 2 + priv->size;
}


#if GTK_CHECK_VERSION (3, 0, 0)
static void
blxo_cell_renderer_icon_render (GtkCellRenderer     *renderer,
                               cairo_t             *cr,
                               GtkWidget           *widget,
                               const GdkRectangle  *background_area,
                               const GdkRectangle  *cell_area,
                               GtkCellRendererState flags)
{
    GdkRectangle        clip_area;
    GdkRectangle       *expose_area = &clip_area;
    GdkRGBA            *color_rgba;
    GdkColor            color_gdk;
    GtkStyleContext    *style_context;
#else
static void
blxo_cell_renderer_icon_render (GtkCellRenderer     *renderer,
                               GdkWindow           *window,
                               GtkWidget           *widget,
                               GdkRectangle        *background_area,
                               GdkRectangle        *cell_area,
                               GdkRectangle        *expose_area,
                               GtkCellRendererState flags)
{
  cairo_t *cr = gdk_cairo_create (window);
  GtkIconSource                    *icon_source;
  GtkStateType                      state;
#endif
  const BlxoCellRendererIconPrivate *priv = blxo_cell_renderer_icon_get_instance_private (BLXO_CELL_RENDERER_ICON (renderer));
  GtkIconTheme                     *icon_theme;
  GdkRectangle                      icon_area;
  GdkRectangle                      draw_area;
  const gchar                      *filename;
  GtkIconInfo                      *icon_info = NULL;
  GdkPixbuf                        *icon = NULL;
  GdkPixbuf                        *temp;
  GError                           *err = NULL;
  gchar                            *display_name = NULL;
  gint                             *icon_sizes;
  gint                              icon_size;
  gint                              n;

#if GTK_CHECK_VERSION (3, 0, 0)
  gdk_cairo_get_clip_rectangle (cr, expose_area);
#endif

  /* verify that we have an icon */
  if (G_UNLIKELY (priv->icon == NULL && priv->gicon == NULL))
    return;

  /* icon may be either an image file or a named icon */
  if (priv->icon != NULL && g_path_is_absolute (priv->icon))
    {
      /* load the icon via the thumbnail database */
      icon = _blxo_thumbnail_get_for_file (priv->icon, (priv->size > 128) ? BLXO_THUMBNAIL_SIZE_LARGE : BLXO_THUMBNAIL_SIZE_NORMAL, &err);
    }
  else if (priv->icon != NULL || priv->gicon != NULL)
    {
      /* determine the best icon size (GtkIconTheme is somewhat messy scaling up small icons) */
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));

      if (priv->icon != NULL)
        {
          icon_sizes = gtk_icon_theme_get_icon_sizes (icon_theme, priv->icon);
          for (icon_size = -1, n = 0; icon_sizes[n] != 0; ++n)
            {
              /* we can use any size if scalable, because we load the file directly */
              if (icon_sizes[n] == -1)
                icon_size = priv->size;
              else if (icon_sizes[n] > icon_size && icon_sizes[n] <= priv->size)
                icon_size = icon_sizes[n];
            }
          g_free (icon_sizes);

          /* if we don't know any icon sizes at all, the icon is probably not present */
          if (G_UNLIKELY (icon_size < 0))
            icon_size = priv->size;

          /* lookup the icon in the icon theme */
          icon_info = gtk_icon_theme_lookup_icon (icon_theme, priv->icon, icon_size, 0);
        }
      else if (priv->gicon != NULL)
        {
          icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                                      priv->gicon,
                                                      priv->size,
                                                      GTK_ICON_LOOKUP_USE_BUILTIN);
        }

      if (G_UNLIKELY (icon_info == NULL))
        return;

      /* check if we have an SVG icon here */
      filename = gtk_icon_info_get_filename (icon_info);
      if (filename != NULL && g_str_has_suffix (filename, ".svg"))
        {
          /* loading SVG icons is terribly slow, so we try to use thumbnail instead, and we use the
           * real available cell area directly here, because loading thumbnails involves scaling anyway
           * and this way we need to the thumbnail pixbuf scale only once.
           */
          icon = _blxo_thumbnail_get_for_file (filename, (priv->size > 128) ? BLXO_THUMBNAIL_SIZE_LARGE : BLXO_THUMBNAIL_SIZE_NORMAL, &err);
        }
      else
        {
          /* regularly load the icon from the theme */
          icon = gtk_icon_info_load_icon (icon_info, &err);
        }
      gtk_icon_info_free (icon_info);
    }

  /* check if we failed */
  if (G_UNLIKELY (icon == NULL))
    {
      /* better let the user know whats going on, might be surprising otherwise */
      if (G_LIKELY (priv->icon != NULL))
        {
          display_name = g_filename_display_name (priv->icon);
        }
      else if (G_UNLIKELY (priv->gicon != NULL
                           && g_object_class_find_property (G_OBJECT_GET_CLASS (priv->gicon),
                                                            "name")))
        {
          g_object_get (priv->gicon, "name", &display_name, NULL);
        }

      if (display_name != NULL)
        {
          g_warning ("Failed to load \"%s\": %s", display_name, err->message);
          g_free (display_name);
        }

      g_error_free (err);
      return;
    }

  /* determine the real icon size */
  icon_area.width = gdk_pixbuf_get_width (icon);
  icon_area.height = gdk_pixbuf_get_height (icon);

  /* scale down the icon on-demand */
  if (G_UNLIKELY (icon_area.width > cell_area->width || icon_area.height > cell_area->height))
    {
      /* scale down to fit */
      temp = blxo_gdk_pixbuf_scale_down (icon, TRUE, cell_area->width, cell_area->height);
      g_object_unref (G_OBJECT (icon));
      icon = temp;

      /* determine the icon dimensions again */
      icon_area.width = gdk_pixbuf_get_width (icon);
      icon_area.height = gdk_pixbuf_get_height (icon);
    }

  icon_area.x = cell_area->x + (cell_area->width - icon_area.width) / 2;
  icon_area.y = cell_area->y + (cell_area->height - icon_area.height) / 2;

  /* Gtk2: check whether the icon is affected by the expose event */
  /* Gtk3: we don't have any expose rectangle and just draw everything */
  if (gdk_rectangle_intersect (expose_area, &icon_area, &draw_area))
    {
      /* colorize the icon if we should follow the selection state */
      if ((flags & (GTK_CELL_RENDERER_SELECTED | GTK_CELL_RENDERER_PRELIT)) != 0 && priv->follow_state)
        {
          if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
            {
#if GTK_CHECK_VERSION (3, 0, 0)
              style_context = gtk_widget_get_style_context (widget);
              gtk_style_context_get (style_context, gtk_widget_has_focus (widget) ? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_ACTIVE,
                                     GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &color_rgba,
                                     NULL);

              color_gdk.pixel = 0;
              color_gdk.red = color_rgba->red * 65535;
              color_gdk.blue = color_rgba->blue * 65535;
              color_gdk.green = color_rgba->green * 65535;
              gdk_rgba_free (color_rgba);
              temp = blxo_gdk_pixbuf_colorize (icon, &color_gdk);
#else
              state = gtk_widget_has_focus (widget) ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE;

              temp = blxo_gdk_pixbuf_colorize (icon, &gtk_widget_get_style (widget)->base[state]);
#endif
              g_object_unref (G_OBJECT (icon));
              icon = temp;
            }

          if ((flags & GTK_CELL_RENDERER_PRELIT) != 0)
            {
              temp = blxo_gdk_pixbuf_spotlight (icon);
              g_object_unref (G_OBJECT (icon));
              icon = temp;
            }
        }

#if GTK_CHECK_VERSION (3, 0, 0)
      /* check if we should render an insensitive icon */
      if (G_UNLIKELY (gtk_widget_get_state_flags(widget) & GTK_STATE_INSENSITIVE || !gtk_cell_renderer_get_sensitive (renderer)))
        {
          style_context = gtk_widget_get_style_context (widget);
          gtk_style_context_get (style_context, GTK_STATE_FLAG_INSENSITIVE,
                                 GTK_STYLE_PROPERTY_COLOR, &color_rgba,
                                 NULL);

          color_gdk.pixel = 0;
          color_gdk.red = color_rgba->red * 65535;
          color_gdk.blue = color_rgba->blue * 65535;
          color_gdk.green = color_rgba->green * 65535;
          gdk_rgba_free (color_rgba);
          temp = blxo_gdk_pixbuf_colorize (icon, &color_gdk);

          g_object_unref (G_OBJECT (icon));
          icon = temp;
        }
#else
      /* check if we should render an insensitive icon */
      if (G_UNLIKELY (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE || !gtk_cell_renderer_get_sensitive (renderer)))
        {
          /* allocate an icon source */
          icon_source = gtk_icon_source_new ();
          gtk_icon_source_set_pixbuf (icon_source, icon);
          gtk_icon_source_set_size_wildcarded (icon_source, FALSE);
          gtk_icon_source_set_size (icon_source, GTK_ICON_SIZE_SMALL_TOOLBAR);

          /* render the insensitive icon */
          temp = gtk_style_render_icon (gtk_widget_get_style (widget), icon_source, gtk_widget_get_direction (widget),
                                        GTK_STATE_INSENSITIVE, -1, widget, "gtkcellrendererpixbuf");
          g_object_unref (G_OBJECT (icon));
          icon = temp;

          /* release the icon source */
          gtk_icon_source_free (icon_source);
        }
#endif

      /* render the invalid parts of the icon */
      gdk_cairo_set_source_pixbuf (cr, icon, icon_area.x, icon_area.y);
      cairo_rectangle (cr, draw_area.x, draw_area.y, draw_area.width, draw_area.height);
      cairo_fill (cr);
    }

  /* release the file's icon */
  g_object_unref (G_OBJECT (icon));

#if !GTK_CHECK_VERSION (3, 0, 0)
  cairo_destroy (cr);
#endif
}



/**
 * blxo_cell_renderer_icon_new:
 *
 * Creates a new #BlxoCellRendererIcon. Adjust rendering parameters using object properties,
 * which can be set globally via g_object_set(). Also, with #GtkCellLayout and
 * #GtkTreeViewColumn, you can bind a property to a value in a #GtkTreeModel. For example
 * you can bind the <link linkend="BlxoCellRendererIcon--icon">icon</link> property on the
 * cell renderer to an icon name in the model, thus rendering a different icon in each row
 * of the #GtkTreeView.
 *
 * Returns: the newly allocated #BlxoCellRendererIcon.
 *
 * Since: 0.3.1.9
 **/
GtkCellRenderer*
blxo_cell_renderer_icon_new (void)
{
  return g_object_new (BLXO_TYPE_CELL_RENDERER_ICON, NULL);
}



#define __BLXO_CELL_RENDERER_ICON_C__
#include <blxo/blxo-aliasdef.c>
