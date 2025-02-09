/*-
 * Copyright (c) 2004 os-cillation e.K.
 * Copyright (c) 2004 Victor Porton (http://ex-code.com/~porton/)
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

#if !defined (BLXO_INSIDE_BLXO_H) && !defined (BLXO_COMPILATION)
#error "Only <blxo/blxo.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __BLXO_BINDING_H__
#define __BLXO_BINDING_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _BlxoBinding       BlxoBinding;
typedef struct _BlxoMutualBinding BlxoMutualBinding;

/**
 * BlxoBindingTransform:
 * @src_value : Value to transform.
 * @dst_value : Value to store the result of the transformation into.
 * @user_data : User data supplied at binding creation.
 *
 * Function type used for binding transformation functions.
 *
 * Accomplished transformation from @src_value to @dst_value.
 * @src_value and @dst_value are already initialized before
 * this function gets called.
 *
 * Returns: %FALSE if transformation failed, else %TRUE.
 **/
typedef gboolean  (*BlxoBindingTransform)  (const GValue *src_value,
                                           GValue       *dst_value,
                                           gpointer      user_data);



BlxoBinding        *blxo_binding_new                      (GObject            *src_object,
                                                         const gchar        *src_property,
                                                         GObject            *dst_object,
                                                         const gchar        *dst_property);
BlxoBinding        *blxo_binding_new_full                 (GObject            *src_object,
                                                         const gchar        *src_property,
                                                         GObject            *dst_object,
                                                         const gchar        *dst_property,
                                                         BlxoBindingTransform transform,
                                                         GDestroyNotify      destroy_notify,
                                                         gpointer            user_data);
BlxoBinding        *blxo_binding_new_with_negation        (GObject            *src_object,
                                                         const gchar        *src_property,
                                                         GObject            *dst_object,
                                                         const gchar        *dst_property);
void               blxo_binding_unbind                   (BlxoBinding         *binding);

BlxoMutualBinding  *blxo_mutual_binding_new               (GObject            *object1,
                                                         const gchar        *property1,
                                                         GObject            *object2,
                                                         const gchar        *property2);
BlxoMutualBinding  *blxo_mutual_binding_new_full          (GObject            *object1,
                                                         const gchar        *property1,
                                                         GObject            *object2,
                                                         const gchar        *property2,
                                                         BlxoBindingTransform transform,
                                                         BlxoBindingTransform reverse_transform,
                                                         GDestroyNotify      destroy_notify,
                                                         gpointer            user_data);
BlxoMutualBinding  *blxo_mutual_binding_new_with_negation (GObject            *object1,
                                                         const gchar        *property1,
                                                         GObject            *object2,
                                                         const gchar        *property2);
void               blxo_mutual_binding_unbind            (BlxoMutualBinding   *binding);

G_END_DECLS

#endif /* !__BLXO_BINDING_H__ */
