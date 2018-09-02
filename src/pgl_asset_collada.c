#include "pgl_asset_collada.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <expat.h>


/* parsing */
static element(collada) * g_collada;

/* globals */
static size_t g_current_depth;
static int g_undefined_element_flag;
static const char* g_current_elem_tag;
static void* g_current_elem;
static ptr_complex_element** g_pending_references;
static size_t g_n_pending_reference;
static int g_parser_status; // 0 : elemstart , 1 : chardata , 2 : elemend

struct collada_elem_attribs {
  const char* id;         
  const char* sid;
  const char* name;
  const char* type;
  const char* source;
  const char* semantic;
  const char* version;
  const char* url;
  const char* xmlns;

  size_t count;
  size_t stride;
  size_t offset;
  size_t set;
  
  double meter;

} g_collada_elem_attribs;

struct collada_elem_attrib_states {
  short id;         
  short sid;
  short name;
  short type;
  short source;
  short semantic;
  short version;
  short url;
  short xmlns;
  
  short count;
  short stride;
  short offset;
  short set;

  short meter;
};
struct collada_elem_attrib_states g_collada_elem_attrib_states;

static void* resize(void *ptr,size_t newsize){
  if(ptr==NULL){
    ptr = malloc(newsize);
    assert(ptr);
  }else{
    void* tmp_ptr = realloc(ptr,newsize);
    assert(tmp_ptr);
    ptr = tmp_ptr;
  }
  return ptr;
}

static void print_elem_value (FILE* file,complex_element* elem){
  if(strcmp(elem->base_type,"list_of_floats")==0){
    double* ptr = *((double**)elem->value_ptr);
    for(int i=0;i<elem->value_size;i++){
      if(i==elem->value_size-1){
	fprintf(file,"%.7g",ptr[i]);
      }else{
	fprintf(file,"%.7g ",ptr[i]);
      }
    }
  } else if(strcmp(elem->base_type,"list_of_ints")==0){
    long int* ptr = *((long int**)elem->value_ptr);
    for(int i=0;i<elem->value_size;i++){
      if(i==elem->value_size-1){
	fprintf(file,"%ld",ptr[i]);
      }else{
	fprintf(file,"%ld ",ptr[i]);
      }
    }
  } else if(strcmp(elem->base_type,"list_of_uints")==0){
    unsigned long* ptr = *((unsigned long**)elem->value_ptr);
    for(int i=0;i<elem->value_size;i++){
      if(i==elem->value_size-1){
	fprintf(file,"%lu",ptr[i]);
      }else{
	fprintf(file,"%lu ",ptr[i]);
      }
    }
  } else if(strcmp(elem->base_type,"float4x4")==0){
    double* ptr = *((double**)elem->value_ptr);
    for(int i=0;i<elem->value_size;i++){
      if(i==elem->value_size-1){
	fprintf(file,"%.7g",ptr[i]);
      }else{
	fprintf(file,"%.7g ",ptr[i]);
      }
    }
  } else if(strcmp(elem->base_type,"float")==0){
    fprintf(file,"%.7g",*(double*)elem->value_ptr);
  } else if(strcmp(elem->base_type,"int")==0){
    fprintf(file,"%ld",*(long int*)elem->value_ptr);
  } else if(strcmp(elem->base_type,"uint")==0){
    fprintf(file,"%lu",*(unsigned long*)elem->value_ptr);
  }
}
			
static void print_attribute(FILE* file,simple_element* elem){
  assert(elem); 
  if(strcmp(elem->base_type,"float")==0){   
    fprintf(file," %s=\"",elem->name);
    fprintf(file,"%f",*((double*)elem->value_ptr));
    fprintf(file,"\"");
  } else if(strcmp(elem->base_type,"int")==0){
    fprintf(file," %s=\"",elem->name);
    fprintf(file,"%d",*((int*)elem->value_ptr));
    fprintf(file,"\"");
  } else if(strcmp(elem->base_type,"uint")==0){
    fprintf(file," %s=\"",elem->name);
    fprintf(file,"%u",*((unsigned int*)elem->value_ptr));
    fprintf(file,"\"");
  } else if(strcmp(elem->base_type,"string")==0){
    if(*((char**)elem->value_ptr)){
    fprintf(file," %s=\"",elem->name);
    fprintf(file,"%s",*((char**)elem->value_ptr));
    fprintf(file,"\"");
    }
  }
}

static void print_element(FILE* file,complex_element* elem, int depth){
  assert(elem);
  complex_element* elem_ptr = (complex_element*)elem;

  fprintf(file,"\n");
  for(int i=0;i<depth;++i){
    fprintf(file,"  ");
  }
  fprintf(file,"<%s",elem_ptr->name);
  for(int i=0;i<elem_ptr->n_attrib;++i){
    print_attribute(file,elem_ptr->attribs[i]);
  }
  if(elem_ptr->n_elem){
    fprintf(file,">");
    for(int i=0;i<elem_ptr->n_elem;++i){
      print_element(file,elem_ptr->elems[i],depth + 1);   
    }
    fprintf(file,"\n");
    for(int i=0;i<depth;++i){
      fprintf(file,"  ");
    }
    fprintf(file,"</%s>",elem_ptr->name);
  }else if(elem_ptr->value_ptr){
    fprintf(file,">");
    print_elem_value(file,elem_ptr);
    fprintf(file,"</%s>",elem_ptr->name);  
  }else{
    fprintf(file,"/>");
  }
}

static void export_dae_file(const char* file_name){
  FILE* file = fopen(file_name,"w");
  if(!file) {
        printf("File opening failed");
        return;
  }
  fprintf(file,"<?xml version=\"1.0\" encoding=\"utf-8\"?>");
  print_element(file,(complex_element*)g_collada,0);
  fclose(file);
}

static void init_simple_element_base(void* obj,char* name,char* base_type,void* parent,void* value_ptr){
  simple_element* this = (simple_element*)obj;
  this->name = NULL;
  this->base_type = NULL;
  this->name =  resize(this->name,strlen(name)*sizeof(char)+sizeof(char));
  strcpy(this->name,name);
  this->base_type =  resize(this->base_type,strlen(base_type)*sizeof(char)+sizeof(char));
  strcpy(this->base_type,base_type);
  this->parent = parent;
  this->value_ptr = value_ptr;

   complex_element* parent_ptr = (complex_element*) parent;

   if(parent){
     int parsed = 1;
     if(strcmp(name,"sid") ==0 && g_collada_elem_attrib_states.sid){//sid
       char** sid  = (char**)this->value_ptr;
       (*sid) = resize( (*sid), strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
       strcpy((*sid),g_collada_elem_attribs.sid);
     }else if(strcmp(name,"id") ==0 && g_collada_elem_attrib_states.id){//id
       char** id  = (char**)this->value_ptr;
       (*id) = resize( (*id), strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
       strcpy((*id),g_collada_elem_attribs.id);
     }else if(strcmp(name,"name") ==0 && g_collada_elem_attrib_states.name){//name
       char** name  = (char**)this->value_ptr;
       (*name) = resize( (*name), strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
       strcpy((*name),g_collada_elem_attribs.name);
     }else if(strcmp(name,"type") ==0 && g_collada_elem_attrib_states.type){//type
       char** type  = (char**)this->value_ptr;
       (*type) = resize( (*type), strlen(g_collada_elem_attribs.type)*sizeof(char)+sizeof(char));
       strcpy((*type),g_collada_elem_attribs.type);
     }else if(strcmp(name,"source") ==0 && g_collada_elem_attrib_states.source){//source
       char** source  = (char**)this->value_ptr;
       (*source) = resize( (*source), strlen(g_collada_elem_attribs.source)*sizeof(char)+sizeof(char));
       strcpy((*source),g_collada_elem_attribs.source);
     }else if(strcmp(name,"semantic") ==0 && g_collada_elem_attrib_states.semantic){//semantic
       char** semantic  = (char**)this->value_ptr;
       (*semantic) = resize( (*semantic), strlen(g_collada_elem_attribs.semantic)*sizeof(char)+sizeof(char));
       strcpy((*semantic),g_collada_elem_attribs.semantic);
     }else if(strcmp(name,"version") ==0 && g_collada_elem_attrib_states.version){//version
       char** version  = (char**)this->value_ptr;
       (*version) = resize( (*version), strlen(g_collada_elem_attribs.version)*sizeof(char)+sizeof(char));
       strcpy((*version),g_collada_elem_attribs.version);
     }else if(strcmp(name,"url") ==0 && g_collada_elem_attrib_states.url){//url
       char** url  = (char**)this->value_ptr;
       (*url) = resize( (*url), strlen(g_collada_elem_attribs.url)*sizeof(char)+sizeof(char));
       strcpy((*url),g_collada_elem_attribs.url);
     }else if(strcmp(name,"xmlns") ==0 && g_collada_elem_attrib_states.xmlns){//xmlns
       char** xmlns  = (char**)this->value_ptr;
       (*xmlns) = resize( (*xmlns), strlen(g_collada_elem_attribs.xmlns)*sizeof(char)+sizeof(char));
       strcpy((*xmlns),g_collada_elem_attribs.xmlns);
     }else if(strcmp(name,"count") ==0 && g_collada_elem_attrib_states.count){//count
       size_t* count = (size_t*)this->value_ptr;
       (*count) = g_collada_elem_attribs.count;
     }else if(strcmp(name,"stride") ==0 && g_collada_elem_attrib_states.stride){//stride
       size_t* stride = (size_t*)this->value_ptr;
       (*stride) = g_collada_elem_attribs.stride;
     }else if(strcmp(name,"offset") ==0 && g_collada_elem_attrib_states.offset){//offset
       size_t* offset = (size_t*)this->value_ptr;
       (*offset) = g_collada_elem_attribs.offset;
     }else if(strcmp(name,"set") ==0 && g_collada_elem_attrib_states.set){//set
       size_t* set = (size_t*)this->value_ptr;
       (*set) = g_collada_elem_attribs.set;
     }else if(strcmp(name,"meter") ==0 && g_collada_elem_attrib_states.meter){//meter
       double* meter = (double*)this->value_ptr;
       (*meter) = g_collada_elem_attribs.meter;
     }else{
       parsed = 0;
     }
     if(parsed){
       parent_ptr->attribs = resize(parent_ptr->attribs,++parent_ptr->n_attrib * sizeof(simple_element*));
       parent_ptr->attribs[parent_ptr->n_attrib - 1] = (simple_element*) this;
     }
   } else{
     printf("parent bos oldugu icin attrib eklenemiyor");
   }
}

static void init_complex_element_base(void* obj,char* name,char* base_type,void* parent,void* value_ptr){
  complex_element* this = (complex_element*)obj;
  this->name = NULL;
  this->base_type = NULL;
  this->name =  resize(this->name,strlen(name)*sizeof(char)+sizeof(char));
  strcpy(this->name,name);
  this->base_type =  resize(this->base_type,strlen(base_type)*sizeof(char)+sizeof(char));
  strcpy(this->base_type,base_type);
  this->parent = parent;
  this->value_ptr = value_ptr;
  this->elem_type = 1;
    
  this->elems = NULL;
  this->attribs = NULL;
  this->refs = NULL;
  this->n_ref = 0;
  this->n_attrib = 0;
  this->n_elem = 0;
  
  complex_element* parent_ptr = (complex_element*) parent;

  if(parent){
    parent_ptr->elems = resize(parent_ptr->elems,++parent_ptr->n_elem * sizeof(complex_element*));
    parent_ptr->elems[parent_ptr->n_elem - 1] = (complex_element*) this;
  }

  g_current_elem = this;
}

static void init_ptr_complex_element_base(void* obj,char* src,char* ptr_type,void* parent){
  ptr_complex_element* this = (ptr_complex_element*)obj;
  this->ptr_type =  resize(this->ptr_type,strlen(ptr_type)*sizeof(char)+sizeof(char));
  strcpy(this->ptr_type,ptr_type);
  this->src = src;
  this->parent = parent;
  this->ptr = NULL;

  complex_element* parent_ptr = (complex_element*) parent;

  if(parent){
    parent_ptr->refs = resize(parent_ptr->refs,++parent_ptr->n_ref * sizeof(ptr_complex_element*));
    parent_ptr->refs[parent_ptr->n_ref - 1] = this;
  }

  g_pending_references = resize(g_pending_references,++g_n_pending_reference * sizeof(ptr_complex_element*));
  g_pending_references[g_n_pending_reference - 1] = this;
}

define_init_function(instance_geometry){
  element(node) * parent = (element(node)*) g_current_elem;
  assert(parent);
  element(instance_geometry)** ptr = parent->ch_instance_geometry;
  ptr = resize( ptr, ++parent->n_instance_geometry * sizeof(element(instance_geometry)*));
  parent->ch_instance_geometry = ptr;
  parent->ch_instance_geometry[parent->n_instance_geometry - 1] = resize( NULL, sizeof(element(instance_geometry)));
  element(instance_geometry)* this = parent->ch_instance_geometry[parent->n_instance_geometry - 1];
  
  this->p_node = parent;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;
  this->a_url.value = NULL;
  
  init_complex_element_base(this,"instance_geometry","none",parent,NULL);
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_url,"url","string",this,&this->a_url.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_ptr_complex_element_base(&this->r_geometry,this->a_url.value,"geometry",this);
}

define_init_function(instance_camera){
  element(node) * parent = (element(node)*) g_current_elem;
  assert(parent);
  element(instance_camera)** ptr = parent->ch_instance_camera;
  ptr = resize( ptr, ++parent->n_instance_camera * sizeof(element(instance_camera)*));
  parent->ch_instance_camera = ptr;
  parent->ch_instance_camera[parent->n_instance_camera - 1] = resize( NULL, sizeof(element(instance_camera)));
  element(instance_camera)* this = parent->ch_instance_camera[parent->n_instance_camera - 1];
  
  this->p_node = parent;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;
  this->a_url.value = NULL;
  
  init_complex_element_base(this,"instance_camera","none",parent,NULL); 
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_url,"url","string",this,&this->a_url.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_ptr_complex_element_base(&this->r_camera,this->a_url.value,"camera",this);
}

define_init_function(matrix){
  element(node) * parent = (element(node)*) g_current_elem;
  assert(parent);
  element(matrix)** ptr = parent->ch_matrix;
  ptr = resize( ptr, ++parent->n_matrix * sizeof(element(matrix)*));
  parent->ch_matrix = ptr;
  parent->ch_matrix[parent->n_matrix - 1] = resize( NULL, sizeof(element(matrix)));
  element(matrix)* this = parent->ch_matrix[parent->n_matrix - 1];

  this->p_node = parent;
  this->a_sid.value = NULL;

  init_complex_element_base(this,"matrix","float4x4",parent,&this->_ext.value);
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
}

define_init_function(node){

  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  element(node)* this = NULL;
  complex_element* parent_ptr  = NULL;
  
  if(strncmp(elem->name,"vi",2) == 0 ){ // visual_scene
    element(visual_scene) * parent = (element(visual_scene)*) g_current_elem;
    element(node)** ptr = parent->ch_node;
    ptr = resize( ptr, ++parent->n_node * sizeof(element(node)*));
    parent->ch_node = ptr;
    parent->ch_node[parent->n_node - 1] = resize( NULL, sizeof(element(node)));
    this = parent->ch_node[parent->n_node - 1];
    
    this->p_visual_scene = parent;
    parent_ptr = (complex_element*) parent;
  } else if (strncmp(elem->name,"no",2) == 0){ // node
    element(node) * parent = (element(node)*) g_current_elem;
    element(node)** ptr = parent->ch_node;
    ptr = resize( ptr, ++parent->n_node * sizeof(element(node)*));
    parent->ch_node = ptr;
    parent->ch_node[parent->n_node - 1] = resize( NULL, sizeof(element(node)));
    this = parent->ch_node[parent->n_node - 1];
    
    this->p_node = parent;
    parent_ptr = (complex_element*) parent;
  }
  assert(this);
  
  this->a_id.value = NULL;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;

  this->ch_lookat = NULL;
  this->ch_matrix = NULL;
  this->ch_rotate = NULL;
  this->ch_scale = NULL;
  this->ch_translate = NULL;
  this->ch_instance_camera = NULL;
  this->ch_instance_geometry = NULL;
  this->ch_instance_node = NULL;
  this->ch_node = NULL;

  this->n_lookat = 0;
  this->n_matrix = 0;
  this->n_rotate = 0;
  this->n_scale = 0;
  this->n_translate = 0;
  this->n_instance_camera = 0;
  this->n_instance_geometry = 0;
  this->n_instance_node = 0;
  this->n_node = 0;

  init_complex_element_base(this,"node","none",parent_ptr,NULL); 
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(p){
  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
 
  if(strcmp(elem->name,"polylist") == 0 ){
    element(polylist) * parent = (element(polylist)*) g_current_elem;
    assert(parent);
    element(p)* this = &parent->c_p;
    this->p_polylist = parent;
    this->_ext.value = NULL;
 
    init_complex_element_base(this,"p","list_of_uints",parent,&this->_ext.value);
  }else {
    g_undefined_element_flag = 1;
  }
}

define_init_function(vcount){
  element(polylist) * parent = (element(polylist)*) g_current_elem;
  assert(parent);
  element(vcount)* this = &parent->c_vcount;
  this->p_polylist = parent;
  this->_ext.value = NULL;

  init_complex_element_base(this,"vcount","list_of_uints",parent,&this->_ext.value);
}

define_init_function(input){
  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  
  if(strncmp(elem->name,"ve",2) == 0 ){ // vertices
    element(vertices) * parent = (element(vertices)*) g_current_elem;
    element(input_local)** ptr = parent->ch_input_local;
    ptr = resize( ptr, ++parent->n_input_local * sizeof(element(input_local)*));
    parent->ch_input_local = ptr;
    parent->ch_input_local[parent->n_input_local - 1] = resize( NULL, sizeof(element(input_local)));
    element(input_local)* this = parent->ch_input_local[parent->n_input_local - 1];
    
    this->p_vertices = parent;
  
    init_complex_element_base(this,"input","none",parent,NULL);
    init_simple_element_base(&this->a_semantic,"semantic","string",this,&this->a_semantic.value);
    init_simple_element_base(&this->a_source,"source","string",this,&this->a_source.value);
    init_ptr_complex_element_base(&this->r_source,this->a_source.value,"source",this);
 
  } else if (strncmp(elem->name,"po",2) == 0){ // polylist
    element(polylist) * parent = (element(polylist)*) g_current_elem;
    element(input_local_offset)** ptr = parent->ch_input_local_offset;
    ptr = resize( ptr, ++parent->n_input_local_offset * sizeof(element(input_local_offset)*));
    parent->ch_input_local_offset = ptr;   
    parent->ch_input_local_offset[parent->n_input_local_offset - 1] = resize( NULL, sizeof(element(input_local_offset)));
    element(input_local_offset)* this = parent->ch_input_local_offset[parent->n_input_local_offset - 1];
    
    this->p_polylist = parent;
    this->a_semantic.value = NULL;
    this->a_source.value = NULL;

    init_complex_element_base(this,"input","none",parent,NULL);
    init_simple_element_base(&this->a_semantic,"semantic","string",this,&this->a_semantic.value);
    init_simple_element_base(&this->a_source,"source","string",this,&this->a_source.value);
    init_simple_element_base(&this->a_set,"set","uint",this,&this->a_set.value);
    init_simple_element_base(&this->a_offset,"offset","uint",this,&this->a_offset.value);
    init_ptr_complex_element_base(&this->r_source,this->a_source.value,"source",this);
    init_ptr_complex_element_base(&this->r_vertices,this->a_source.value,"source",this);
  }else{
    g_undefined_element_flag = 1;
  }
}

define_init_function(param){
  element(accessor) * parent = (element(accessor)*) g_current_elem;
  element(param)** ptr = parent->ch_param;
  ptr = resize( ptr, ++parent->n_param * sizeof(element(param)*));
  parent->ch_param = ptr;
  parent->ch_param[parent->n_param - 1] = resize( NULL, sizeof(element(param)));
  element(param)* this = parent->ch_param[parent->n_param - 1];
  
  this->p_accessor = parent;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;
  this->a_type.value = NULL;
  this->a_semantic.value = NULL;

  init_complex_element_base(this,"param","none",parent,NULL);
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_simple_element_base(&this->a_semantic,"semantic","string",this,&this->a_semantic.value);
  init_simple_element_base(&this->a_type,"type","string",this,&this->a_type.value);

}

define_init_function(accessor){
  element(source_technique_common) * parent = (element(source_technique_common)*) g_current_elem;
  element(accessor) * this = &parent->c_accessor;
  this->p_source_technique_common = parent;

  this->a_source.value = NULL;
  this->ch_param = NULL;
  this->n_param = 0;

  init_complex_element_base(this,"accessor","none",parent,NULL);
  init_simple_element_base(&this->a_source,"source","string",this,&this->a_source.value);
  init_simple_element_base(&this->a_count,"count","uint",this,&this->a_count.value);
  init_simple_element_base(&this->a_stride,"stride","uint",this,&this->a_stride.value);
  init_simple_element_base(&this->a_offset,"offset","uint",this,&this->a_offset.value);
  init_ptr_complex_element_base(&this->r_float_array,this->a_source.value,"float_array",this);
  init_ptr_complex_element_base(&this->r_int_array,this->a_source.value,"int_array",this);
}

define_init_function(int_array){
  element(source) * parent = (element(source)*) g_current_elem;
  element(int_array) * this = &parent->c_int_array;
  this->p_source = parent;
  this->a_name.value = NULL;
  this->a_id.value = NULL;
  this->_ext.value = NULL;

  init_complex_element_base(this,"int_array","list_of_ints",parent,&this->_ext.value); 
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_simple_element_base(&this->a_count,"count","uint",this,&this->a_count.value);
}

define_init_function(float_array){
  element(source) * parent = (element(source)*) g_current_elem;
  element(float_array) * this = &parent->c_float_array;
  this->p_source = parent;
  this->a_name.value = NULL;
  this->a_id.value = NULL;
  this->_ext.value = NULL;

  init_complex_element_base(this,"float_array","list_of_floats",parent,&this->_ext.value); 
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_simple_element_base(&this->a_count,"count","uint",this,&this->a_count.value);
}

define_init_function(polylist){
  element(mesh) * parent = (element(mesh)*) g_current_elem;
  element(polylist)** ptr = parent->ch_polylist;
  ptr = resize( ptr, ++parent->n_polylist * sizeof(element(polylist)*));
  parent->ch_polylist = ptr;
  parent->ch_polylist[parent->n_polylist - 1] = resize( NULL, sizeof(element(polylist)));
  element(polylist)* this = parent->ch_polylist[parent->n_polylist - 1];

  this->p_mesh = parent;
  this->a_name.value = NULL;
  this->ch_input_local_offset = NULL;
  this->n_input_local_offset = 0;

  init_complex_element_base(this,"polylist","none",parent,NULL);
  init_simple_element_base(&this->a_count,"count","uint",this,&this->a_count.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(vertices){
  element (mesh) * parent = (element(mesh)*) g_current_elem;
  element(vertices) * this = &parent->c_vertices;
  this->p_mesh = parent;
  
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->ch_input_local = NULL;
  this->n_input_local = 0;

  init_complex_element_base(this,"vertices","none",parent,NULL);
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value); 
}

define_init_function(source){
  element(mesh) * parent = (element(mesh)*) g_current_elem;
  element(source)** ptr = parent->ch_source;
  ptr = resize( ptr, ++parent->n_source * sizeof(element(source)*));
  parent->ch_source = ptr;
  parent->ch_source[parent->n_source - 1] = resize( NULL, sizeof(element(source)));
  element(source)* this = parent->ch_source[parent->n_source - 1];

  this->p_mesh = parent;
  this->a_id.value = NULL;
  this->a_name.value = NULL;

  init_complex_element_base(this,"source","none",parent,NULL);
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(mesh){
  element(geometry) * parent = (element(geometry)*) g_current_elem;
  assert(parent);
  element(mesh)* this = &parent->c_mesh;
  this->p_geometry = parent;

  this->ch_source = NULL;
  this->ch_polylist = NULL;
  this->n_source = 0;
  this->n_polylist = 0;
  
  init_complex_element_base(this,"mesh","none",parent,NULL);
}

define_init_function(aspect_ratio){
  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  element(aspect_ratio)* this = NULL;
  complex_element* parent_ptr = NULL;
  
  if(strncmp(elem->name,"pe",2) == 0 ){ // perspective
    element(perspective) * parent = (element(perspective)*) g_current_elem;
    this = &parent->c_aspect_ratio;
    this->p_perspective = parent;
    parent_ptr = (complex_element*) parent;
  } else if (strncmp(elem->name,"or",2) == 0){ // orthographic
    element(orthographic) * parent = (element(orthographic)*) g_current_elem;
    this = &parent->c_aspect_ratio;
    this->p_orthographic = parent;
    parent_ptr = (complex_element*) parent;
  }
  assert(this);
  this->_ext.a_sid.value = NULL;

  init_complex_element_base(this,"aspect_ratio","float",parent_ptr,&this->_ext._ext.value); 
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
}

define_init_function(zfar){
  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  element(zfar)* this = NULL;
  complex_element* parent_ptr = NULL;
  if(strncmp(elem->name,"pe",2) == 0 ){ // perspective
    element(perspective) * parent = (element(perspective)*) g_current_elem;
    this = &parent->c_zfar;
    this->p_perspective = parent;
    parent_ptr = (complex_element*)parent;
  } else if (strncmp(elem->name,"or",2) == 0){ // orthographic
    element(orthographic) * parent = (element(orthographic)*) g_current_elem;
    this = &parent->c_zfar;
    this->p_orthographic = parent;
    parent_ptr = (complex_element*)parent;
  }
  assert(this);
  this->_ext.a_sid.value = NULL;
  
  init_complex_element_base(this,"zfar","float",parent_ptr,&this->_ext._ext.value);
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
}

define_init_function(znear){
  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  element(znear)* this = NULL;
  complex_element* parent_ptr = NULL;
  if(strncmp(elem->name,"pe",2) == 0 ){ // perspective
    element(perspective) * parent = (element(perspective)*) g_current_elem;
    this = &parent->c_znear;
    this->p_perspective = parent;
    parent_ptr = (complex_element*)parent;
  } else if (strncmp(elem->name,"or",2) == 0){ // orthographic
    element(orthographic) * parent = (element(orthographic)*) g_current_elem;
    this = &parent->c_znear;
    this->p_orthographic = parent;
    parent_ptr = (complex_element*)parent;
  }
  assert(this);
  this->_ext.a_sid.value = NULL;

  init_complex_element_base(this,"znear","float",parent_ptr,&this->_ext._ext.value); 
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
}

define_init_function(ymag){
  element(orthographic) * parent = (element(orthographic)*) g_current_elem;
  element(ymag)* this = &parent->c_ymag;
  this->p_orthographic = parent;
  this->_ext.a_sid.value = NULL;

  init_complex_element_base(this,"ymag","float",parent,&this->_ext._ext.value);
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
}

define_init_function(xmag){
  element(orthographic) * parent = (element(orthographic)*) g_current_elem;
  element(xmag)* this = &parent->c_xmag;
  this->p_orthographic = parent;
  this->_ext.a_sid.value = NULL;

  init_complex_element_base(this,"xmag","float",parent,&this->_ext._ext.value);
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value); 
}

define_init_function(yfov){
  element(perspective) * parent = (element(perspective)*) g_current_elem;
  element(yfov)* this = &parent->c_yfov;
  this->p_perspective = parent;
  this->_ext.a_sid.value = NULL;
  
  init_complex_element_base(this,"yfov","float",parent,&this->_ext._ext.value); 
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value); 
}

define_init_function(xfov){
  element(perspective) * parent = (element(perspective)*) g_current_elem;
  element(xfov)* this = &parent->c_xfov;
  this->p_perspective = parent;
  this->_ext.a_sid.value = NULL;
 
  init_complex_element_base(this,"xfov","float",parent,&this->_ext._ext.value);
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
}

define_init_function(orthographic){
  element(optics_technique_common) * parent = (element(optics_technique_common)*) g_current_elem;
  element(orthographic)* this = &parent->c_orthographic;
  this->p_optics_technique_common = parent;

  init_complex_element_base(this,"orthographic","none",parent,NULL);
}

define_init_function(perspective){
  element(optics_technique_common) * parent = (element(optics_technique_common)*) g_current_elem;
  element(perspective)* this = &parent->c_perspective;

  init_complex_element_base(this,"perspective","none",parent,NULL);
}

define_init_function(technique_common){
  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  if(strncmp(elem->name,"op",2) == 0 ){ // optics
    element(optics) * parent = (element(optics)*) g_current_elem;
    element(optics_technique_common)* this = &parent->c_optics_technique_common;
    this->p_optics = parent;
  
    init_complex_element_base(this,"technique_common","none",parent,NULL);  
  } else if (strncmp(elem->name,"so",2) == 0){ // source
    element(source) * parent = (element(source)*) g_current_elem;
    element(source_technique_common)* this = &parent->c_source_technique_common;
    this->p_source = parent;
    
    init_complex_element_base(this,"technique_common","none",parent,NULL);    
  }else{
    g_undefined_element_flag = 1;
  }
}

define_init_function(optics){
  element(camera) * parent = (element(camera)*) g_current_elem;
  element(optics)* this = &parent->c_optics;
  this->p_camera = parent;

  init_complex_element_base(this,"optics","none",parent,NULL);
}

define_init_function(instance_visual_scene){
  element(scene) * parent = (element(scene)*) g_current_elem;
  element(instance_visual_scene) * this = &parent->c_instance_visual_scene;
  this->p_scene = parent;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;
  this->a_url.value = NULL;
 
  init_complex_element_base(this,"instance_visual_scene","none",parent,NULL);
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_simple_element_base(&this->a_url,"url","string",this,&this->a_url.value);
  init_ptr_complex_element_base(&this->r_visual_scene,this->a_url.value,"visual_scene",this);
}

define_init_function(visual_scene){
  element(library_visual_scenes) * parent = (element(library_visual_scenes)*) g_current_elem;
  element(visual_scene)** ptr = parent->ch_visual_scene;
  ptr = resize( ptr, ++parent->n_visual_scene * sizeof(element(visual_scene)*));
  parent->ch_visual_scene = ptr;
  parent->ch_visual_scene[parent->n_visual_scene - 1] = resize( NULL, sizeof(element(visual_scene)));
  element(visual_scene)* this = parent->ch_visual_scene[parent->n_visual_scene - 1];
  
  this->p_library_visual_scenes = parent;
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->ch_node = NULL;
  this->n_node = 0;

  init_complex_element_base(this,"visual_scene","none",parent,NULL);
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(geometry){
  element(library_geometries) * parent = (element(library_geometries)*) g_current_elem;
  element(geometry)** ptr = parent->ch_geometry;
  ptr = resize( ptr, ++parent->n_geometry * sizeof(element(geometry)*));
  parent->ch_geometry = ptr;
  parent->ch_geometry[parent->n_geometry - 1] = resize( NULL, sizeof(element(geometry)));
  element(geometry)* this = parent->ch_geometry[parent->n_geometry - 1];
  
  this->p_library_geometries = parent;
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  
  init_complex_element_base(this,"geometry","none",parent,NULL); 
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(camera){
  element(library_cameras) * parent = (element(library_cameras)*) g_current_elem;
  element(camera)** ptr = parent->ch_camera;
  ptr = resize( ptr, ++parent->n_camera * sizeof(element(camera)*));
  parent->ch_camera = ptr;
  parent->ch_camera[parent->n_camera - 1] = resize( NULL, sizeof(element(camera)));
  element(camera)* this = parent->ch_camera[parent->n_camera - 1];
  
  this->p_library_cameras = parent;	     
  this->a_id.value=NULL;
  this->a_name.value=NULL;
 
  init_complex_element_base(this,"camera","none",parent,NULL);
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(library_cameras){
  element(collada) * parent = (element(collada)*) g_current_elem;
  element(library_cameras)** ptr = parent->ch_library_cameras;
  ptr = resize( ptr, ++parent->n_library_cameras * sizeof(element(library_cameras)*));
  parent->ch_library_cameras = ptr;

  parent->ch_library_cameras[parent->n_library_cameras - 1] = resize( NULL, sizeof(element(library_cameras)));
  element(library_cameras)* this = parent->ch_library_cameras[parent->n_library_cameras - 1];
  
  this->p_collada = parent;  
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->n_camera = 0;
  this->ch_camera = NULL;
  
  init_complex_element_base(this,"library_cameras","none",parent,NULL);
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(library_geometries){
  element(collada) * parent = (element(collada)*) g_current_elem;
  element(library_geometries)** ptr = parent->ch_library_geometries;
  ptr = resize( ptr, ++parent->n_library_geometries * sizeof(element(library_geometries)*));
  parent->ch_library_geometries = ptr;
  parent->ch_library_geometries[parent->n_library_geometries - 1] = resize( NULL, sizeof(element(library_geometries)));
  element(library_geometries)* this = parent->ch_library_geometries[parent->n_library_geometries - 1];

  this->p_collada = parent;  
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->n_geometry = 0;
  this->ch_geometry = NULL;
 
  init_complex_element_base(this,"library_geometries","none",parent,NULL); 
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(library_visual_scenes){
  element(collada) * parent = (element(collada)*) g_current_elem;
  element(library_visual_scenes)** ptr = parent->ch_library_visual_scenes;
  ptr = resize( ptr, ++parent->n_library_visual_scenes * sizeof(element(library_visual_scenes)*));
  parent->ch_library_visual_scenes = ptr;
  parent->ch_library_visual_scenes[parent->n_library_visual_scenes - 1] = resize( NULL, sizeof(element(library_visual_scenes)));
  element(library_visual_scenes)* this = parent->ch_library_visual_scenes[parent->n_library_visual_scenes - 1];
  
  this->p_collada = parent;
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->n_visual_scene = 0;
  this->ch_visual_scene = NULL;

  init_complex_element_base(this,"library_visual_scenes","none",parent,NULL); 
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
}

define_init_function(scene){
  element(collada) * parent = (element(collada)*) g_current_elem;
  assert(parent);
  element(scene) * this = &parent->c_scene;
  this->p_collada = parent;
  
  init_complex_element_base(this,"scene","none",parent,NULL);
}

define_init_function(collada){
  // bu satirda collada root oldugu icin cast yapmaya gerek yok
  element(collada)* this = g_collada;
  //
  this = resize( this, sizeof(element(collada)));
  this->a_version.value = NULL;
  this->a_xmlns.value = NULL;
  this->ch_library_geometries = NULL;
  this->ch_library_cameras = NULL;
  this->ch_library_visual_scenes = NULL;
  this->n_library_geometries = 0;
  this->n_library_cameras = 0;
  this->n_library_visual_scenes = 0;

  init_complex_element_base(this,"COLLADA","none",NULL,NULL);
  init_simple_element_base(&this->a_xmlns,"xmlns","string",this,&this->a_xmlns.value);
  init_simple_element_base(&this->a_version,"version","string",this,&this->a_version.value);

  g_collada = this;  
}

static void reset_parsed_attrib_states(){
  g_collada_elem_attrib_states = (struct collada_elem_attrib_states) {
    .id = 0,     
    .sid = 0,
    .name = 0,
    .type = 0,
    .version = 0,
    .url = 0,
    .xmlns = 0,
    .stride = 0,
    .offset = 0,
    .set = 0,
    .meter = 0 
  };
}

static void parse_attribs(void* userdata, const char** attr){
  size_t nattr=XML_GetSpecifiedAttributeCount((XML_Parser) userdata);
  size_t i=0;
  const char* ptr = NULL;
  
  if(nattr){
    for(i=0;i<nattr/2;++i){
      ptr=attr[i*2];
      if(ptr[0]=='i'){//id
	ptr=attr[i*2+1];
	g_collada_elem_attribs.id = ptr;
	g_collada_elem_attrib_states.id = 1;
      }else if(ptr[0]=='n'){//name
	ptr=attr[i*2+1];
	g_collada_elem_attribs.name = ptr;
	g_collada_elem_attrib_states.name = 1;
      }else if(ptr[0]=='s'){
	if(ptr[1]=='i'){//sid
	  ptr=attr[i*2+1];
	  g_collada_elem_attribs.sid = ptr;
	  g_collada_elem_attrib_states.sid = 1;
	}else if(ptr[1]=='e'){
	  if(ptr[2]=='t'){//set
	    ptr=attr[i*2+1];
	    g_collada_elem_attribs.set = strtoul(ptr,NULL,10);
	    g_collada_elem_attrib_states.set = 1;
	  }else if(ptr[2]=='m'){//semantic
	    ptr=attr[i*2+1];
	    g_collada_elem_attribs.semantic = ptr;
	    g_collada_elem_attrib_states.semantic = 1;
	  }
	}else if(ptr[1]=='o'){//source
	  ptr=attr[i*2+1];
	  g_collada_elem_attribs.source = ptr;
	  g_collada_elem_attrib_states.source = 1;
	}else if(ptr[1]=='t'){//stride
	  ptr=attr[i*2+1];
	  g_collada_elem_attribs.stride = strtoul(ptr,NULL,10);
	  g_collada_elem_attrib_states.stride = 1;
	}
      }else if(ptr[0]=='t'){//type
	ptr=attr[i*2+1];
	g_collada_elem_attribs.type = ptr;
	g_collada_elem_attrib_states.type = 1;
      }else if(ptr[0]=='u'){//url
	ptr=attr[i*2+1];
	g_collada_elem_attribs.url = ptr;
	g_collada_elem_attrib_states.url = 1;
      }else if(ptr[0]=='m'){//meter
	ptr=attr[i*2+1];
	g_collada_elem_attribs.meter = strtod(ptr,NULL);
	g_collada_elem_attrib_states.meter = 1;
      }else if(ptr[0]=='v'){//version
	ptr=attr[i*2+1];
	g_collada_elem_attribs.version = ptr;
	g_collada_elem_attrib_states.version = 1;
      }else if(ptr[0]=='c'){//count
	ptr=attr[i*2+1];
	g_collada_elem_attribs.count = strtoul(ptr,NULL,10);
	g_collada_elem_attrib_states.count = 1;
      }else if(ptr[0]=='o'){//offset
	ptr=attr[i*2+1];
	g_collada_elem_attribs.offset = strtoul(ptr,NULL,10);
	g_collada_elem_attrib_states.offset = 1;
      } else if(ptr[0]=='x'){//xmlns
	ptr=attr[i*2+1];
	g_collada_elem_attribs.xmlns = ptr;
	g_collada_elem_attrib_states.xmlns = 1;
      }
    }
  }

}

static void collada(elemend)(void *userdata,const char *elem){
  g_undefined_element_flag = 0;
  g_parser_status = 2;
  g_current_depth--;
  
  if(g_current_depth==5){
    if(strncmp(elem,"per",3)==0){
    }else if(strncmp(elem,"ort",3)==0){
    }else if(strncmp(elem,"float_",6)==0){
    }else if(strncmp(elem,"int_",4)==0){
    }else if(strncmp(elem,"technique_c",11)==0){
    }else if(strncmp(elem,"inp",3)==0){
    }else if(strcmp(elem,"p")==0){
    }else if(strncmp(elem,"vc",2)==0){
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==4){
    if(strncmp(elem,"technique_c",11)==0){
    }else if(strcmp(elem,"source")==0){
    }else if(strncmp(elem,"verti",5)==0){
    }else if(strncmp(elem,"polyl",5)==0){
    }else if(strncmp(elem,"matr",4)==0){
    }else if(strncmp(elem,"instance_ca",11)==0){
    }else if(strncmp(elem,"instance_g",10)==0){
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==6){
    if(strncmp(elem,"xf",2)==0){
    }else if(strncmp(elem,"yf",2)==0){
    }else if(strncmp(elem,"asp",3)==0){
    }else if(strncmp(elem,"zn",2)==0){
    }else if(strcmp(elem,"zfar")==0){
    }else if(strncmp(elem,"ac",2)==0){
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==1){
    if(strncmp(elem,"library_ca",10)==0){
    }else if(strncmp(elem,"library_ge",10)==0){
    }else if(strncmp(elem,"library_vi",10)==0){
    }else if(strncmp(elem,"sce",3)==0){
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==2){
    if(strncmp(elem,"up",2)==0){
    }else if(strncmp(elem,"cam",3)==0){
    }else if(strncmp(elem,"geo",3)==0){
    }else if(strncmp(elem,"vis",3)==0){
    }else if(strncmp(elem,"instance_v",10)==0){
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==3){
    if(strncmp(elem,"optic",5)==0){
    }else if(strncmp(elem,"mes",3)==0){
    }else if(strncmp(elem,"nod",3)==0){
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==0){
    if(strncmp(elem,"CO",2)==0){
      puts(((simple_element*) g_current_elem)->name);
      g_current_depth--;
      return;
     }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==7){
    if(strncmp(elem,"par",3)==0){
    }else{
      g_undefined_element_flag = 1;
    }
  }
  g_current_depth++;
  if(!g_undefined_element_flag){
    puts(((simple_element*) g_current_elem)->name);
    g_current_elem = ((simple_element*) g_current_elem)->parent;
    g_current_depth--;
  }
}

static unsigned long* parse_list_of_uints_tail(size_t* value_size,const XML_Char* str,int len){
  char* end = NULL;
  const char* old_ptr = str;
  int i = 0;
  for (unsigned long f = strtoul(str, &end,10); end <= (old_ptr + len * sizeof(char)); f = strtoul(str, &end,10)){
    if(end==str){ //  If no conversion can be performed
      break;
    }
    str = end;
    if (errno == ERANGE){
      printf("range error, got ");
      errno = 0;
    }else{
      i++;
    }
  }
  if(i==0){
    *value_size = 0;
    return NULL;
  }
  unsigned long* arr = NULL;
  arr = resize(arr,i * sizeof(unsigned long));
  i=0;
  str = old_ptr;
  end = NULL;
  for (unsigned long f = strtoul(str, &end,10); end <= (old_ptr + len * sizeof(char)); f = strtoul(str, &end,10)){
    if(end==str){ //  If no conversion can be performed
      break;
    }
    str = end;
    if (errno == ERANGE){
      printf("range error, got ");
      errno = 0;
    }else{
      *(arr+i) = f;
      i++;
    }
  }
  *value_size = i;
  return arr;
}

static long int* parse_list_of_ints_tail(size_t* value_size,const XML_Char* str,int len){
  char* end = NULL;
  const char* old_ptr = str;
  int i = 0;
  for (long int f = strtol(str, &end,10); end <= (old_ptr + len * sizeof(char)); f = strtol(str, &end,10)){
    if(end==str){ //  If no conversion can be performed
      break;
    }
    str = end;
    if (errno == ERANGE){
      printf("range error, got ");
      errno = 0;
    }else{
      i++;
    }
  }
  if(i==0){
    *value_size = 0;
    return NULL;
  }
  long int* arr = NULL;
  arr = resize(arr,i * sizeof(long int));
  i=0;
  str = old_ptr;
  end = NULL;
  for (long int f = strtol(str, &end,10); end <= (old_ptr + len * sizeof(char)); f = strtol(str, &end,10)){
    if(end==str){ //  If no conversion can be performed
      break;
    }
    str = end;
    if (errno == ERANGE){
      printf("range error, got ");
      errno = 0;
    }else{
      *(arr+i) = f;
      i++;
    }
  }
  *value_size = i;
  return arr;
}

static double* parse_list_of_floats_tail(size_t* value_size,const XML_Char* str,int len){
  char* end = NULL;
  const char* old_ptr = str;
  int i = 0;
  for (double f = strtod(str, &end); end <= (old_ptr + len * sizeof(char)); f = strtod(str, &end)){
    if(end==str){ //  If no conversion can be performed
      break;
    }
    str = end;
    if (errno == ERANGE){
      printf("range error, got ");
      errno = 0;
    }else{
      i++;
    }
  }
  if(i==0){
    *value_size = 0;
    return NULL;
  }
  double* arr = NULL;
  arr = resize(arr,i * sizeof(double));
  i=0;
  str = old_ptr;
  end = NULL;
  for (double f = strtod(str, &end); end <= (old_ptr + len * sizeof(char)); f = strtod(str, &end)){
    if(end==str){ //  If no conversion can be performed
      break;
    }
    str = end;
    if (errno == ERANGE){
      printf("range error, got ");
      errno = 0;
    }else{
      *(arr+i) = f;
      i++;
    }
  }
  *value_size = i;
  return arr;
}

static void parse_list_of_floats(complex_element* this,int merge_data_flag,const XML_Char* string,int len){
  double** value_ptr = (double**)this->value_ptr;
  if(merge_data_flag){
    size_t size = 0;
    double* buffer = parse_list_of_floats_tail(&size,string,len);
    if(size != 0){    
      size_t new_size = size + this->value_size;
      (*value_ptr) = resize((*value_ptr), new_size * sizeof(double));
      assert(buffer);
      for(int i = this->value_size, j = 0 ; i < new_size; i++,j++ ){
	(*value_ptr)[i] = buffer[j];
      }
      this->value_size = new_size;
    }
  }else{	
    size_t size = 0;
    double* buffer = parse_list_of_floats_tail(&size,string,len);
    if(size){
      (*value_ptr) = buffer;
      this->value_size = size;
    }
  }
}

static void parse_list_of_ints(complex_element* this,int merge_data_flag,const XML_Char* string,int len){
  long int** value_ptr = (long int**)this->value_ptr;
  if(merge_data_flag){
    size_t size = 0;
    long int* buffer = parse_list_of_ints_tail(&size,string,len);
    if(size != 0){    
      size_t new_size = size + this->value_size;
      (*value_ptr) = resize((*value_ptr), new_size * sizeof(long int));
      assert(buffer);
      for(int i = this->value_size, j = 0 ; i < new_size; i++,j++ ){
	(*value_ptr)[i] = buffer[j];
      }
      this->value_size = new_size;
    }
  }else{	
    size_t size = 0;
    long int* buffer = parse_list_of_ints_tail(&size,string,len);
    if(size){
      (*value_ptr) = buffer;
      this->value_size = size;
    }
  }
}

static void parse_list_of_uints(complex_element* this,int merge_data_flag,const XML_Char* string,int len){
  unsigned long** value_ptr = (unsigned long**)this->value_ptr;
  if(merge_data_flag){
    size_t size = 0;
    unsigned long* buffer = parse_list_of_uints_tail(&size,string,len);
    if(size != 0){    
      size_t new_size = size + this->value_size;
      (*value_ptr) = resize((*value_ptr), new_size * sizeof(unsigned long));
      assert(buffer);
      for(int i = this->value_size, j = 0 ; i < new_size; i++,j++ ){
	(*value_ptr)[i] = buffer[j];
      }
      this->value_size = new_size;
    }
  }else{
    size_t size = 0;
    unsigned long* buffer = parse_list_of_uints_tail(&size,string,len);
    if(size){
      (*value_ptr) = buffer;
      this->value_size = size;
    }
  }
}

static void parse_double(complex_element* this,int merge_data_flag,const XML_Char* string,int len){
  double* value_ptr = (double*)this->value_ptr;
  if(merge_data_flag){
    if(this->value_size == 0){
      size_t size = 0;
      double* buffer = parse_list_of_floats_tail(&size,string,len);
      if(size != 0){    
	assert(buffer);
	(*value_ptr) = buffer[0];
	this->value_size = 1;
      }
    }
  }else{
    size_t size = 0;
    double* buffer = parse_list_of_floats_tail(&size,string,len);
    if(size){
      (*value_ptr) = buffer[0];
    }
  }
}

static void collada(chardata)(void *userdata,const XML_Char *string,int len){
  int data_parsed = 1;
  int merge_data_flag = 0;
  // bu fonksiyonda string komple geldigi icin parse edilirken kontroller eklenecek !
  if(g_parser_status == 1){ // eger onceki asamada chardata calistiysa onceki ile burdaki string i birlestir
    merge_data_flag = 1;
  }
     
  if(g_current_depth==6){
    if(strncmp(g_current_elem_tag,"float_",6)==0){
      parse_list_of_floats((complex_element*)g_current_elem,merge_data_flag,string,len);     
    }else if(strncmp(g_current_elem_tag,"int_",4)==0){
      parse_list_of_ints((complex_element*)g_current_elem,merge_data_flag,string,len);    
    }else if(strcmp(g_current_elem_tag,"p")==0){
      parse_list_of_uints((complex_element*)g_current_elem,merge_data_flag,string,len);
    }else if(strncmp(g_current_elem_tag,"vc",2)==0){
      parse_list_of_uints((complex_element*)g_current_elem,merge_data_flag,string,len);
    }else{
      data_parsed = 0;
    }
  }else if(g_current_depth==5){
    if(strncmp(g_current_elem_tag,"matr",4)==0){
      parse_list_of_floats((complex_element*)g_current_elem,merge_data_flag,string,len);
    }else{
      data_parsed = 0;
    }
  }else if(g_current_depth==7){
    if(strncmp(g_current_elem_tag,"xf",2)==0){
      parse_double((complex_element*)g_current_elem,merge_data_flag,string,len);
    }else if(strncmp(g_current_elem_tag,"yf",2)==0){
      parse_double((complex_element*)g_current_elem,merge_data_flag,string,len);
    }else if(strncmp(g_current_elem_tag,"asp",3)==0){
      parse_double((complex_element*)g_current_elem,merge_data_flag,string,len);
    }else if(strncmp(g_current_elem_tag,"zn",2)==0){
      parse_double((complex_element*)g_current_elem,merge_data_flag,string,len);
    }else if(strcmp(g_current_elem_tag,"zfar")==0){
      parse_double((complex_element*)g_current_elem,merge_data_flag,string,len);
    }else{
      data_parsed = 0;
    }
  }else if(g_current_depth==3){
    if(strncmp(g_current_elem_tag,"up",2)==0){
    }else{
      data_parsed = 0;
    }
  }else{
    data_parsed = 0;
  }
  
  if(data_parsed){ 
    for(int tab_count=0;tab_count<g_current_depth;++tab_count){
      printf("  ");
    }
    printf("%.*s\n",len,string);
  }

  g_parser_status = 1;

}

static void collada(elemstart)(void *userdata,const char *elem,const char **attr){
  g_current_elem_tag = elem;
  g_undefined_element_flag = 0;
  g_parser_status = 0;

  reset_parsed_attrib_states();

  // burdaki string karsilastirmasi icin hash fonksiyonu bulunacak
  // bu kisimda herhangi bir element icin parse_attribs calistirilmaz ise onceki degerler kaliyor
  // o yuzden ya parse_attribs en ustte bir kez cagirilacak yada dikkatli sekilde her biri iicn ayri ayri cagirilacak
  if(g_current_depth==5){
    if(strncmp(elem,"per",3)==0){
      collada(init_perspective)();
    }else if(strncmp(elem,"ort",3)==0){
      collada(init_orthographic)();
    }else if(strncmp(elem,"float_",6)==0){
      parse_attribs(userdata,attr);
      collada(init_float_array)();
    }else if(strncmp(elem,"int_",4)==0){
      parse_attribs(userdata,attr);
      collada(init_int_array)();
    }else if(strncmp(elem,"technique_c",11)==0){
      collada(init_technique_common)();
    }else if(strncmp(elem,"inp",3)==0){
      parse_attribs(userdata,attr);
      collada(init_input)();
    }else if(strcmp(elem,"p")==0){
      collada(init_p)();
    }else if(strncmp(elem,"vc",2)==0){
      collada(init_vcount)();
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==4){
    if(strncmp(elem,"technique_c",11)==0){
      collada(init_technique_common)();
    }else if(strcmp(elem,"source")==0){
      parse_attribs(userdata,attr);
      collada(init_source)();
    }else if(strncmp(elem,"verti",5)==0){
      parse_attribs(userdata,attr);
      collada(init_vertices)();
    }else if(strncmp(elem,"polyl",5)==0){
      parse_attribs(userdata,attr);
      collada(init_polylist)();
    }else if(strncmp(elem,"matr",4)==0){
      parse_attribs(userdata,attr);
      collada(init_matrix)();
    }else if(strncmp(elem,"instance_ca",11)==0){
      parse_attribs(userdata,attr);
      collada(init_instance_camera)();
    }else if(strncmp(elem,"instance_g",10)==0){
      parse_attribs(userdata,attr);
      collada(init_instance_geometry)();
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==6){
    if(strncmp(elem,"xf",2)==0){
      parse_attribs(userdata,attr);
      collada(init_xfov)();
    }else if(strncmp(elem,"yf",2)==0){
      parse_attribs(userdata,attr);
      collada(init_yfov)();
    }else if(strncmp(elem,"asp",3)==0){
      parse_attribs(userdata,attr);
      collada(init_aspect_ratio)();
    }else if(strncmp(elem,"zn",2)==0){
      parse_attribs(userdata,attr);
      collada(init_znear)();
    }else if(strcmp(elem,"zfar")==0){
      parse_attribs(userdata,attr);
      collada(init_zfar)();
    }else if(strncmp(elem,"ac",2)==0){
      parse_attribs(userdata,attr);
      collada(init_accessor)();
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==1){
    if(strncmp(elem,"library_ca",10)==0){
      parse_attribs(userdata,attr);      
      collada(init_library_cameras)();
    }else if(strncmp(elem,"library_ge",10)==0){
      parse_attribs(userdata,attr);      
      collada(init_library_geometries)();
    }else if(strncmp(elem,"library_vi",10)==0){
      parse_attribs(userdata,attr);      
      collada(init_library_visual_scenes)();
    }else if(strncmp(elem,"sce",3)==0){
      collada(init_scene)();
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==2){
    if(strncmp(elem,"up",2)==0){
      
    }else if(strncmp(elem,"cam",3)==0){
      parse_attribs(userdata,attr);      
      collada(init_camera)();
    }else if(strncmp(elem,"geo",3)==0){
      parse_attribs(userdata,attr);      
      collada(init_geometry)();
    }else if(strncmp(elem,"vis",3)==0){
      parse_attribs(userdata,attr);
      collada(init_visual_scene)();
    }else if(strncmp(elem,"instance_v",10)==0){
      parse_attribs(userdata,attr);  
      collada(init_instance_visual_scene)();
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==3){
    if(strncmp(elem,"optic",5)==0){
      collada(init_optics)();
    }else if(strncmp(elem,"mes",3)==0){
      collada(init_mesh)();
    }else if(strncmp(elem,"nod",3)==0){
      parse_attribs(userdata,attr);
      collada(init_node)();
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==0){
    if(strncmp(elem,"CO",2)==0){
      parse_attribs(userdata,attr);
      collada(init_collada)();
    }else{
      g_undefined_element_flag = 1;
    }
  }else if(g_current_depth==7){
    if(strncmp(elem,"par",3)==0){
      parse_attribs(userdata,attr);
      collada(init_param)();
    }else{
      g_undefined_element_flag = 1;
    }
  }

  if(!g_undefined_element_flag){
    g_current_depth++;
  }
}

static int resolve_refs(complex_element* root,int n_resolved){
  
  int resolved = 0;
  
  for(int i=0;i<root->n_elem;++i){
    if(n_resolved + resolved == g_n_pending_reference){
      return resolved;
    }else{
      resolved += resolve_refs(root->elems[i],n_resolved + resolved);
    }
  }

  if(n_resolved + resolved == g_n_pending_reference){
    return n_resolved + resolved;
  }else{

    for(int k=0; k < g_n_pending_reference; k++){

      if(g_pending_references[k]->src[0] == '#'){
	// URI Addressing
	
	for(int j=0;j<root->n_attrib;++j){      
	  if(strcmp("id",root->attribs[j]->name) == 0){     
	    if(strcmp((g_pending_references[k]->src + sizeof(char)),*((char**)root->attribs[j]->value_ptr) ) == 0){
	      printf("\nfound ref %s",root->name);	      
	      g_pending_references[k]->ptr = root;
	      return 1 + resolved;
	    }
	  }
	}
	
      }else{
	// SID Addressing
	
	// not implemented yet	
      }
        
    }
   
  }

  return resolved;

}


void collada(parse)(const char *filename){
  XML_Parser p;
  FILE *f;
  long fsize;
  char *string;
  size_t i;
  /* init local */
  string=NULL;
  fsize=0;
  f=NULL;


  /* read dae file */
  f=fopen(filename, "rb");
  if(!f){
    puts ("import edilecek dosya bulunamadi");

    /* return NULL; */
  }else{
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  //same as rewind(f);

    string = malloc(fsize + 1);
    assert(string);
    if(!fread(string, fsize, 1, f)){
      puts("dosya okunamadi");
      fclose(f);
    }else{
      fclose(f);

      string[fsize] = 0;
      /* puts(string); */

      /* init globals */

      g_current_depth = 0;
      g_pending_references = NULL;
      g_n_pending_reference = 0;
    
      
      printf("\n-------------PARSING DOCUMENT\n");

      /* create parser and parse */
      p = XML_ParserCreate(NULL);
      assert(p);
      XML_UseParserAsHandlerArg(p);
      XML_SetElementHandler(p, element(elemstart),element(elemend));
      XML_SetCharacterDataHandler(p, element(chardata));
      
      if (!XML_Parse(p, string, fsize, -1)) {
	fprintf(stderr, "Parse error at line %lu:\n%s\n",XML_GetCurrentLineNumber(p),XML_ErrorString(XML_GetErrorCode(p)));    
      }else {

	printf("\n\n\n\n");
	printf("\n-------------EXPORTING DOCUMENT\n");
	printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	print_element(stdout,(complex_element*)g_collada,0);

	export_dae_file("exported.dae");

	printf("\n\n\n\n");
	printf("\n-----------------RESOLVING REFERENCES\n");
	printf("pending refs = %lu",g_n_pending_reference);
	int resolved = resolve_refs((complex_element*)g_collada,0);

	printf("\nresolved refs = %d",resolved);

      
	printf("\n\n\n\n");
      }
      
      /* cleaning */
      XML_ParserFree(p);
      p=NULL;
      free(string);
      string=NULL;

    }
  }
}
