#ifndef _pgl_asset_collada_h
#define _pgl_asset_collada_h

#include <stddef.h>

#define collada(NAME)				\
  pgl_asset_collada_ ## NAME

#define attribute(NAME)				\
  pgl_asset_collada_ ## NAME

#define element(NAME)				\
  pgl_asset_collada_ ## NAME

#define children(TYPE)				\
  element( TYPE ) * 

#define define_attrib(TYPE,NAME)		\
  attribute( TYPE ) a_ ## NAME

#define define_child(TYPE)			\
  element( TYPE ) c_ ## TYPE

#define define_parent(TYPE)			\
  element( TYPE ) * p_ ## TYPE

#define define_ref(TYPE)			\
  element( TYPE ) * r_ ## TYPE

#define define_children(TYPE)			\
  element( TYPE ) ** ch_ ## TYPE ;		\
  size_t n_ ## TYPE				\

#define begin_complex_type(NAME)		\
  struct element( NAME ) {			\
  complex_element _base;

#define begin_simple_type(NAME)			\
  struct element( NAME ) {			\
  simple_element _base;

#define end_type() };

#define extend_type(BASE)			\
  element( BASE ) _ext;

#define alias_element(NAME)				\
  typedef struct element( NAME ## _t ) element( NAME )

#define alias_attrib(NAME)				\
  typedef struct attribute( NAME ## _t ) attribute( NAME )

#define define_init_function(NAME)			\
  static void pgl_asset_collada_init_ ## NAME ()	\


/* root element */
alias_element(collada);
/* modularity elements */
alias_element(library_geometries);
/* geometry elements */
alias_element(geometry);
alias_element(mesh);
/* data flow elements */
alias_element(source);
alias_element(source_technique_common);
alias_element(accessor);
alias_element(param);
/* collation elements */
alias_element(p);
alias_element(vcount);
alias_element(polylist);
alias_element(vertices);
alias_element(library_cameras);
/* object elements */
alias_element(camera);
alias_element(optics);
alias_element(optics_technique_common);
alias_element(perspective);
alias_element(orthographic);
alias_element(xmag);
alias_element(ymag);
alias_element(xfov);
alias_element(yfov);
alias_element(znear);
alias_element(zfar);
alias_element(aspect_ratio);
alias_element(library_nodes);
alias_element(library_visual_scenes);
/* hierarchical elemets */
alias_element(visual_scene);
alias_element(node);
alias_element(instance_geometry);
alias_element(instance_camera);
alias_element(instance_node);
/* transformational elements */
alias_element(lookat);
alias_element(matrix);
alias_element(rotate);
alias_element(scale);
alias_element(translate);
alias_element(scene);
/* instance elements */
alias_element(instance_visual_scene);
/* complex types */
alias_element(input_local_offset);
alias_element(input_local);

/* typed array elements */
alias_element(float_array);
alias_element(int_array);

alias_element(targetable_float);
alias_element(targetable_float3);
alias_element(targetable_float4);

alias_attrib(float);
alias_attrib(int);
alias_attrib(uint);
alias_attrib(float3);
alias_attrib(float4);
alias_attrib(string);
alias_attrib(list_of_floats);
alias_attrib(list_of_ints);
alias_attrib(list_of_uints);
alias_attrib(float3x3);
alias_attrib(float4x4);
alias_attrib(version_enum);
alias_attrib(up_axis_enum);
alias_attrib(node_type_enum);

typedef struct simple_element_t simple_element;
typedef struct complex_element_t complex_element;

struct simple_element_t {
  char* name;
  char* base_type;
  short elem_type;
  void* parent;

  void* value_ptr;
  unsigned int value_size;
};

struct complex_element_t {
  char* name;
  char* base_type;
  short elem_type;
  void* parent;

  void* value_ptr;
  unsigned int value_size;
  
  size_t n_elem;
  size_t n_attrib;
  simple_element** elems;
  simple_element** attribs;
};


begin_simple_type(float_t)
double value;
end_type()

begin_simple_type(int_t)
int value;
end_type()

begin_simple_type(uint_t)
unsigned int value;
end_type()

begin_simple_type(float3_t)
double value[3];
end_type()

begin_simple_type(float4_t)
double value[4];
end_type()

begin_simple_type(float3x3_t)
double value[3][3];
end_type()

begin_simple_type(float4x4_t)
double value[4][4];
end_type()

begin_simple_type(string_t)
char* value;
end_type()

begin_simple_type(list_of_floats_t)
double* value;
end_type()

begin_simple_type(list_of_ints_t)
int* value;
end_type()

begin_simple_type(list_of_uints_t)
unsigned int* value;
end_type()

begin_simple_type(version_enum_t)
enum { V_1_5_0 } value;
end_type()

begin_simple_type(up_axis_enum_t)
enum { X_UP , Y_UP , Z_UP } value;
end_type()

begin_simple_type(node_type_enum_t)
enum { JOINT , NODE } value;
end_type()


begin_complex_type(targetable_float_t) extend_type(float);

define_attrib(string,sid);

end_type()


begin_complex_type(targetable_float3_t) extend_type(float3);

define_attrib(string,sid);

end_type()

begin_complex_type(targetable_float4_t) extend_type(float4);

define_attrib(string,sid);

end_type()


begin_complex_type(input_local_t)
  
define_attrib(string,semantic);
define_attrib(string,source);

define_parent(vertices);

end_type()


begin_complex_type(input_local_offset_t)
  
define_attrib(uint,set);
define_attrib(uint,offset);
define_attrib(string,semantic);
define_attrib(string,source);

define_parent(polylist);

end_type()


begin_complex_type(vcount_t) extend_type(list_of_uints);

define_parent(polylist);

end_type()

begin_complex_type(p_t) extend_type(list_of_uints);

define_parent(polylist);

end_type()

begin_complex_type(float_array_t) extend_type(list_of_floats);

define_attrib(string,id);
define_attrib(string,name);
define_attrib(uint,count);

define_parent(source);

end_type()


begin_complex_type(int_array_t) extend_type(list_of_ints);

define_attrib(string,id);
define_attrib(string,name);
define_attrib(uint,count);

define_parent(source);

end_type()


begin_complex_type(param_t)

define_attrib(string,sid);
define_attrib(string,name);
define_attrib(string,type);
define_attrib(string,semantic);

define_parent(accessor);

end_type()


begin_complex_type(accessor_t)

define_attrib(uint,count);
define_attrib(uint,offset);
define_attrib(uint,stride);
define_attrib(string,source);

define_children(param);

define_ref(source);

define_parent(source_technique_common);

end_type()


begin_complex_type(source_technique_common_t)
  
define_child(accessor);

define_parent(source);

end_type()


begin_complex_type(source_t)

define_attrib(string,id);
define_attrib(string,name);

define_child(float_array);
define_child(int_array);
define_child(source_technique_common);

define_parent(mesh);

end_type()


begin_complex_type(polylist_t)

define_attrib(string,name);
define_attrib(uint,count);

define_children(input_local_offset);
define_child(vcount);
define_child(p);

define_parent(mesh);

end_type()


begin_complex_type(vertices_t)

define_attrib(string,id);
define_attrib(string,name);

define_children(input_local);
  
define_parent(mesh);

end_type()


begin_complex_type(mesh_t)

define_children(source);
define_children(polylist);
define_child(vertices);
  
define_parent(geometry);

end_type()


begin_complex_type(znear_t) extend_type(targetable_float);
define_parent(perspective);
define_parent(orthographic);
end_type()

begin_complex_type(zfar_t) extend_type(targetable_float);
define_parent(perspective);
define_parent(orthographic);
end_type()

begin_complex_type(xmag_t) extend_type(targetable_float);
define_parent(orthographic);
end_type()

begin_complex_type(ymag_t) extend_type(targetable_float);
define_parent(orthographic);
end_type()

begin_complex_type(yfov_t) extend_type(targetable_float);
define_parent(perspective);
end_type()

begin_complex_type(xfov_t) extend_type(targetable_float);
define_parent(perspective);
end_type()

begin_complex_type(aspect_ratio_t) extend_type(targetable_float);
define_parent(perspective);
define_parent(orthographic);
end_type()


begin_complex_type(orthographic_t)

define_child(xmag);
define_child(ymag);
define_child(aspect_ratio);
define_child(znear);
define_child(zfar);

define_parent(optics_technique_common);

end_type()


begin_complex_type(perspective_t)

define_child(xfov);
define_child(yfov);
define_child(aspect_ratio);
define_child(znear);
define_child(zfar);

define_parent(optics_technique_common);

end_type()


begin_complex_type(optics_technique_common_t)

define_child(perspective);
define_child(orthographic);

define_parent(optics);

end_type()


begin_complex_type(optics_t)

define_child(optics_technique_common);

define_parent(camera);

end_type()


begin_complex_type(camera_t)

define_attrib(string,id);
define_attrib(string,name);

define_child(optics);

define_parent(library_cameras);

end_type()


begin_complex_type(geometry_t)

define_attrib(string,id);
define_attrib(string,name);

define_child(mesh);

define_parent(library_geometries);

end_type()


begin_complex_type(lookat_t) extend_type(float3x3);

define_attrib(string,sid);

define_parent(node);

end_type()


begin_complex_type(matrix_t) extend_type(float4x4);

define_attrib(string,sid);

define_parent(node);

end_type()


begin_complex_type(rotate_t) extend_type(targetable_float4);

define_attrib(string,sid);
  
define_parent(node);

end_type()


begin_complex_type(scale_t) extend_type(targetable_float3);

define_attrib(string,sid);

define_parent(node);

end_type()


begin_complex_type(translate_t) extend_type(targetable_float3);

define_attrib(string,sid);

define_parent(node);

end_type()


begin_complex_type(instance_camera_t)

define_attrib(string,sid);
define_attrib(string,name);
define_attrib(string,url);

define_ref(camera);

define_parent(node);

end_type()


begin_complex_type(instance_geometry_t)

define_attrib(string,sid);
define_attrib(string,name);
define_attrib(string,url);

define_ref(geometry);

define_parent(node);

end_type()


begin_complex_type(instance_node_t)

define_attrib(string,sid);
define_attrib(string,name);
define_attrib(string,url);

define_ref(node);

define_parent(node);


end_type()


begin_complex_type(instance_visual_scene_t)

define_attrib(string,sid);
define_attrib(string,name);
define_attrib(string,url);

define_ref(visual_scene);
  
define_parent(scene);

end_type()


begin_complex_type(library_cameras_t)

define_attrib(string,id);
define_attrib(string,name);

define_children(camera);

define_parent(collada);

end_type()


begin_complex_type(library_geometries_t)

define_attrib(string,id);
define_attrib(string,name);

define_children(geometry);

define_parent(collada);

end_type()


begin_complex_type(library_visual_scenes_t)

define_attrib(string,id);
define_attrib(string,name);

define_children(visual_scene);

define_parent(collada);

end_type()


begin_complex_type(node_t)

define_attrib(string,id);
define_attrib(string,name);
define_attrib(string,sid);


define_children(lookat);
define_children(matrix);
define_children(rotate);
define_children(scale);
define_children(translate);
define_children(instance_camera);
define_children(instance_geometry);
define_children(instance_node);
define_children(node);
  

define_parent(node);
define_parent(visual_scene);

end_type()


begin_complex_type(visual_scene_t)

define_attrib(string,id);
define_attrib(string,name);

define_children(node);

define_parent(library_visual_scenes);

end_type()


begin_complex_type(scene_t)
  
define_child(instance_visual_scene);

define_parent(collada);

end_type()


begin_complex_type(collada_t)

define_attrib(string,version);

define_children(library_geometries);
define_children(library_cameras);
define_children(library_visual_scenes);
define_child(scene);

end_type()



#endif
