#include "pgl_asset_collada.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <expat.h>


/* parsing */
static element(collada) * g_collada;

static size_t g_current_depth;
static int g_undefined_element_flag;
static const char* g_current_elem_tag;
static void* g_current_elem;

struct collada_elem_attribs {
  const char* id;         
  const char* sid;
  const char* name;
  const char* type;
  const char* source;
  const char* semantic;
  const char* version;
  const char* url;

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
  
  short count;
  short stride;
  short offset;
  short set;

  short meter;
};
struct collada_elem_attrib_states g_collada_elem_attrib_states;

static void* resize(void *ptr,size_t newsize){
  if(ptr==NULL){
    ptr=malloc(newsize);
  }else{
    void* tmp_ptr = realloc(ptr,newsize);
    assert(tmp_ptr);
    ptr = tmp_ptr;
  }
  assert(ptr);
  return ptr;
}

static void print_elem_value (simple_element* elem){
  if(strcmp(elem->base_type,"list_of_floats")==0){
    double* ptr = *((double**)elem->value_ptr);
    for(int i=0;i<elem->value_size;i++){
      if(i==elem->value_size-1){
	printf("%.7g",ptr[i]);
      }else{
	printf("%.7g ",ptr[i]);
      }
    }
  } else if(strcmp(elem->base_type,"list_of_ints")==0){
    int* ptr = *((int**)elem->value_ptr);
    for(int i=0;i<elem->value_size;i++){
      if(i==elem->value_size-1){
	printf("%d",ptr[i]);
      }else{
	printf("%d ",ptr[i]);
      }
    }
  } else if(strcmp(elem->base_type,"list_of_uints")==0){
    unsigned int* ptr = *((unsigned int**)elem->value_ptr);
    for(int i=0;i<elem->value_size;i++){
      if(i==elem->value_size-1){
	printf("%u",ptr[i]);
      }else{
	printf("%u ",ptr[i]);
      }
    }
  } else if(strcmp(elem->base_type,"float4x4")==0){
    double (*ptr)[4][4]  = (double(*)[4][4]) elem->value_ptr;
    for (int i=0;i<4;i++){
      for(int j=0;j<4;j++){
	if(i==3 && j==3){
	  printf("%.7g",(*ptr)[i][j]);
	}else{
	  printf("%.7g ",(*ptr)[i][j]);
	}
      }
    }
  } else if(strcmp(elem->base_type,"float")==0){
    printf("%.7g",*(double*)elem->value_ptr);
  } else if(strcmp(elem->base_type,"int")==0){
    printf("%d",*(int*)elem->value_ptr);
  } else if(strcmp(elem->base_type,"uint")==0){
    printf("%ud",*(unsigned int*)elem->value_ptr);
  }
}
			
static void print_attribute(simple_element* elem){
  assert(elem); 
  if(strcmp(elem->base_type,"float")==0){   
    printf(" %s=\"",elem->name);
    printf("%f",*((double*)elem->value_ptr));
    printf("\" ");
  } else if(strcmp(elem->base_type,"int")==0){
    printf(" %s=\"",elem->name);
    printf("%d",*((int*)elem->value_ptr));
    printf("\" ");
  } else if(strcmp(elem->base_type,"uint")==0){
    printf(" %s=\"",elem->name);
    printf("%u",*((unsigned int*)elem->value_ptr));
    printf("\" ");
  } else if(strcmp(elem->base_type,"string")==0){
    if(*((char**)elem->value_ptr)){
    printf(" %s=\"",elem->name);
    printf("%s",*((char**)elem->value_ptr));
    printf("\" ");
    }
  }

  
}

static void print_element(simple_element* elem, int depth){
  assert(elem);
  complex_element* elem_ptr = (complex_element*)elem;

  printf("\n");
  for(int i=0;i<depth;++i){
    printf("  ");
  }
  printf("<%s",elem_ptr->name);

  for(int i=0;i<elem_ptr->n_attrib;++i){
    print_attribute(elem_ptr->attribs[i]);
  }

  
  if(elem_ptr->n_elem){

    printf(">");

    for(int i=0;i<elem_ptr->n_elem;++i){
      print_element(elem_ptr->elems[i],depth + 1);   
    }

    printf("\n");
    for(int i=0;i<depth;++i){
      printf("  ");
    }
    printf("</%s>",elem_ptr->name);

  }else if(elem_ptr->value_ptr){
    
    printf(">");
    print_elem_value((simple_element*)elem_ptr);
    printf("</%s>",elem_ptr->name);
    
  }else{
    printf("/>");
  }

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
}

static void init_complex_element_base(void* obj,char* name,char* base_type,void* parent,void* value_ptr,simple_element** elems,simple_element** attribs,size_t n_elem,size_t n_attrib){
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

  complex_element* parent_ptr = (complex_element*) parent;

  if(parent){
    parent_ptr->elems = resize(parent_ptr->elems,++parent_ptr->n_elem * sizeof(simple_element*));
    parent_ptr->elems[parent_ptr->n_elem - 1] = (simple_element*) this;
  }

  if(n_elem) {
    assert(elems);
    this->elems = elems;
    this->n_elem = n_elem;
  }else{
    this->n_elem = 0;
    this->elems = NULL;
  }
  
  if(n_attrib){
    assert(attribs);
    this->attribs = attribs;
    this->n_attrib = n_attrib;
  }else{
    this->n_attrib = 0;
    this->attribs = NULL;
  }
}

define_init_function(instance_geometry){
  element(node) * parent = (element(node)*) g_current_elem;
  assert(parent);
  element(instance_geometry)** ptr = parent->ch_instance_geometry;
  ptr = resize( ptr, ++parent->n_instance_geometry * sizeof(element(instance_geometry)*));
  parent->ch_instance_geometry = ptr;
  element(instance_geometry)* this = parent->ch_instance_geometry[parent->n_instance_geometry - 1];
  this = resize( this, sizeof(element(instance_geometry)));
  
  this->p_node = parent;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;
  this->a_url.value = NULL;

  int n_attrib = 3;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_sid;
  attribs[1] = (simple_element*) &this->a_name;
  attribs[2] = (simple_element*) &this->a_url;

  if(g_collada_elem_attrib_states.sid){
    this->a_sid.value = resize( this->a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->a_sid.value,g_collada_elem_attribs.sid);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  if(g_collada_elem_attrib_states.url){
    this->a_url.value = resize( this->a_url.value, strlen(g_collada_elem_attribs.url)*sizeof(char)+sizeof(char));
    strcpy(this->a_url.value,g_collada_elem_attribs.url);
  }
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_url,"url","string",this,&this->a_url.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"instance_geometry","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(instance_camera){
  element(node) * parent = (element(node)*) g_current_elem;
  assert(parent);
  element(instance_camera)** ptr = parent->ch_instance_camera;
  ptr = resize( ptr, ++parent->n_instance_camera * sizeof(element(instance_camera)*));
  parent->ch_instance_camera = ptr;
  element(instance_camera)* this = parent->ch_instance_camera[parent->n_instance_camera - 1];
  this = resize( this, sizeof(element(instance_camera)));

  this->p_node = parent;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;
  this->a_url.value = NULL;

  int n_attrib = 3;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_sid;
  attribs[1] = (simple_element*) &this->a_name;
  attribs[2] = (simple_element*) &this->a_url;

  if(g_collada_elem_attrib_states.sid){
    this->a_sid.value = resize( this->a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->a_sid.value,g_collada_elem_attribs.sid);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  if(g_collada_elem_attrib_states.url){
    this->a_url.value = resize( this->a_url.value, strlen(g_collada_elem_attribs.url)*sizeof(char)+sizeof(char));
    strcpy(this->a_url.value,g_collada_elem_attribs.url);
  }
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_url,"url","string",this,&this->a_url.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"instance_camera","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(matrix){
  element(node) * parent = (element(node)*) g_current_elem;
  assert(parent);
  element(matrix)** ptr = parent->ch_matrix;
  ptr = resize( ptr, ++parent->n_matrix * sizeof(element(matrix)*));
  parent->ch_matrix = ptr;
  element(matrix)* this = parent->ch_matrix[parent->n_matrix - 1];
  this = resize( this, sizeof(element(matrix)));

  this->p_node = parent;
  this->a_sid.value = NULL;

  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_sid;

  if(g_collada_elem_attrib_states.sid){
    this->a_sid.value = resize( this->a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->a_sid.value,g_collada_elem_attribs.sid);
  }
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_complex_element_base(this,"matrix","float4x4",parent,&this->_ext.value,NULL,NULL,0,0);
  g_current_elem = this;
}

define_init_function(node){

  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  element(node) * this = NULL;
  complex_element* parent_ptr  = NULL;
  
  if(strncmp(elem->name,"vi",2) == 0 ){ // visual_scene
    element(visual_scene) * parent = (element(visual_scene)*) g_current_elem;
    element(node)** ptr = parent->ch_node;
    ptr = resize( ptr, ++parent->n_node * sizeof(element(node)*));
    parent->ch_node = ptr;
    this = parent->ch_node[parent->n_node - 1];
    this = resize( this, sizeof(element(node)));
      
    this->p_visual_scene = parent;
    parent_ptr = (complex_element*) parent;
  } else if (strncmp(elem->name,"no",2) == 0){ // node
    element(node) * parent = (element(node)*) g_current_elem;
    element(node)** ptr = parent->ch_node;
    ptr = resize( ptr, ++parent->n_node * sizeof(element(node)*));
    parent->ch_node = ptr;
    this = parent->ch_node[parent->n_node - 1];
    this = resize( this, sizeof(element(node)));
       
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

  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_sid;
  attribs[1] = (simple_element*) &this->a_name;
    
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.sid){
    this->a_sid.value = resize( this->a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->a_sid.value,g_collada_elem_attribs.sid);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"node","none",parent_ptr,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(p){
  element(polylist) * parent = (element(polylist)*) g_current_elem;
  assert(parent);
  element(p)* this = &parent->c_p;
  this->p_polylist = parent;
  this->_ext.value = NULL;
 
  init_complex_element_base(this,"p","list_of_uints",parent,&this->_ext.value,NULL,NULL,0,0);
  g_current_elem = this;
}

define_init_function(vcount){
  element(polylist) * parent = (element(polylist)*) g_current_elem;
  assert(parent);
  element(vcount)* this = &parent->c_vcount;
  this->p_polylist = parent;
  this->_ext.value = NULL;

  init_complex_element_base(this,"vcount","list_of_uints",parent,&this->_ext.value,NULL,NULL,0,0);
  g_current_elem = this;
}

define_init_function(input){

  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  
  if(strncmp(elem->name,"ve",2) == 0 ){ // vertices
    element(vertices) * parent = (element(vertices)*) g_current_elem;
    element(input_local)** ptr = parent->ch_input_local;
    ptr = resize( ptr, ++parent->n_input_local * sizeof(element(input_local)*));
    parent->ch_input_local = ptr;
    element(input_local)* this = parent->ch_input_local[parent->n_input_local - 1];
    this = resize( this, sizeof(element(input_local)));
    
    this->p_vertices = parent;
  
    int n_attrib = 2;
    simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
    attribs[0] = (simple_element*) &this->a_semantic;
    attribs[1] = (simple_element*) &this->a_source;
    
    if(g_collada_elem_attrib_states.semantic){
      this->a_semantic.value = resize( this->a_semantic.value, strlen(g_collada_elem_attribs.semantic)*sizeof(char)+sizeof(char));
      strcpy(this->a_semantic.value,g_collada_elem_attribs.semantic);
    }
    if(g_collada_elem_attrib_states.source){
      this->a_source.value = resize( this->a_source.value, strlen(g_collada_elem_attribs.source)*sizeof(char)+sizeof(char));
      strcpy(this->a_source.value,g_collada_elem_attribs.source);
    }
    init_simple_element_base(&this->a_semantic,"semantic","string",this,&this->a_semantic.value);
    init_simple_element_base(&this->a_source,"source","string",this,&this->a_source.value);
    init_complex_element_base(this,"input","none",parent,NULL,NULL,attribs,0,n_attrib);
    g_current_elem = this;
    
  } else if (strncmp(elem->name,"po",2) == 0){ // polylist
    element(polylist) * parent = (element(polylist)*) g_current_elem;
    element(input_local_offset)** ptr = parent->ch_input_local_offset;
    ptr = resize( ptr, ++parent->n_input_local_offset * sizeof(element(input_local_offset)*));
    parent->ch_input_local_offset = ptr;
    element(input_local_offset)* this = parent->ch_input_local_offset[parent->n_input_local_offset - 1];
    this = resize( this, sizeof(element(input_local_offset)));

    this->p_polylist = parent;
    this->a_semantic.value = NULL;
    this->a_source.value = NULL;
  
    int n_attrib = 4;
    simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
    attribs[0] = (simple_element*) &this->a_semantic;
    attribs[1] = (simple_element*) &this->a_source;
    attribs[2] = (simple_element*) &this->a_set;
    attribs[3] = (simple_element*) &this->a_offset;

    if(g_collada_elem_attrib_states.offset){
      this->a_offset.value = g_collada_elem_attribs.offset;
    }
    if(g_collada_elem_attrib_states.set){
      this->a_set.value = g_collada_elem_attribs.set;
    }
    if(g_collada_elem_attrib_states.semantic){
      this->a_semantic.value = resize( this->a_semantic.value, strlen(g_collada_elem_attribs.semantic)*sizeof(char)+sizeof(char));
      strcpy(this->a_semantic.value,g_collada_elem_attribs.semantic);
    }
    if(g_collada_elem_attrib_states.source){
      this->a_source.value = resize( this->a_source.value, strlen(g_collada_elem_attribs.source)*sizeof(char)+sizeof(char));
      strcpy(this->a_source.value,g_collada_elem_attribs.source);
    }
    init_simple_element_base(&this->a_semantic,"semantic","string",this,&this->a_semantic.value);
    init_simple_element_base(&this->a_source,"source","string",this,&this->a_source.value);
    init_simple_element_base(&this->a_set,"set","uint",this,&this->a_set.value);
    init_simple_element_base(&this->a_offset,"offset","uint",this,&this->a_offset.value);
    init_complex_element_base(this,"input","none",parent,NULL,NULL,attribs,0,n_attrib);
    g_current_elem = this;
  }
}

define_init_function(param){
  element(accessor) * parent = (element(accessor)*) g_current_elem;
  element(param)** ptr = parent->ch_param;
  ptr = resize( ptr, ++parent->n_param * sizeof(element(param)*));
  parent->ch_param = ptr;
  element(param)* this = parent->ch_param[parent->n_param - 1];
  this = resize( this, sizeof(element(param)));
  
  this->p_accessor = parent;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;
  this->a_type.value = NULL;
  this->a_semantic.value = NULL;
  
  int n_attrib = 4;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_semantic;
  attribs[1] = (simple_element*) &this->a_sid;
  attribs[2] = (simple_element*) &this->a_type;
  attribs[3] = (simple_element*) &this->a_name;

  if(g_collada_elem_attrib_states.sid){
    this->a_sid.value = resize( this->a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->a_sid.value,g_collada_elem_attribs.sid);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  if(g_collada_elem_attrib_states.type){
    this->a_type.value = resize( this->a_type.value, strlen(g_collada_elem_attribs.type)*sizeof(char)+sizeof(char));
    strcpy(this->a_type.value,g_collada_elem_attribs.type);
  }
  if(g_collada_elem_attrib_states.semantic){
    this->a_semantic.value = resize( this->a_semantic.value, strlen(g_collada_elem_attribs.semantic)*sizeof(char)+sizeof(char));
    strcpy(this->a_semantic.value,g_collada_elem_attribs.semantic);
  }
  init_simple_element_base(&this->a_semantic,"semantic","string",this,&this->a_semantic.value);
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_type,"type","string",this,&this->a_type.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"param","none",parent,NULL,NULL,attribs,0,n_attrib);
  
  g_current_elem = this;
}

define_init_function(accessor){
  element(source_technique_common) * parent = (element(source_technique_common)*) g_current_elem;
  element(accessor) * this = &parent->c_accessor;
  this->p_source_technique_common = parent;

  this->a_source.value = NULL;
  this->r_source = NULL;
  this->ch_param = NULL;
  
  int n_attrib = 4;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_source;
  attribs[1] = (simple_element*) &this->a_offset;
  attribs[2] = (simple_element*) &this->a_stride;
  attribs[3] = (simple_element*) &this->a_count;
  
  if(g_collada_elem_attrib_states.count){
    this->a_count.value = g_collada_elem_attribs.count;
  }
  if(g_collada_elem_attrib_states.stride){
    this->a_stride.value = g_collada_elem_attribs.stride;
  }
  if(g_collada_elem_attrib_states.offset){
    this->a_offset.value = g_collada_elem_attribs.offset;
  }
  if(g_collada_elem_attrib_states.source){
    this->a_source.value = resize( this->a_source.value, strlen(g_collada_elem_attribs.source)*sizeof(char)+sizeof(char));
    strcpy(this->a_source.value,g_collada_elem_attribs.source);
  }
  init_simple_element_base(&this->a_count,"count","uint",this,&this->a_count.value);
  init_simple_element_base(&this->a_stride,"stride","uint",this,&this->a_stride.value);
  init_simple_element_base(&this->a_offset,"offset","uint",this,&this->a_offset.value);
  init_simple_element_base(&this->a_source,"source","string",this,&this->a_source.value);
  init_complex_element_base(this,"accessor","none",parent,NULL,NULL,attribs,0,n_attrib);
  
  g_current_elem = this;
}

define_init_function(int_array){
  element(source) * parent = (element(source)*) g_current_elem;
  element(int_array) * this = &parent->c_int_array;
  this->p_source = parent;
  this->a_name.value = NULL;
  this->a_id.value = NULL;
  this->_ext.value = NULL;

  int n_attrib = 3;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_count;
  attribs[1] = (simple_element*) &this->a_id;
  attribs[2] = (simple_element*) &this->a_name;

  
  if(g_collada_elem_attrib_states.count){
    this->a_count.value = g_collada_elem_attribs.count;
  }
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_count,"count","uint",this,&this->a_count.value);
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"int_array","list_of_ints",parent,&this->_ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(float_array){
  element(source) * parent = (element(source)*) g_current_elem;
  element(float_array) * this = &parent->c_float_array;
  this->p_source = parent;
  this->a_name.value = NULL;
  this->a_id.value = NULL;
  this->_ext.value = NULL;

  int n_attrib = 3;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_count;
  attribs[1] = (simple_element*) &this->a_id;
  attribs[2] = (simple_element*) &this->a_name;

  if(g_collada_elem_attrib_states.count){
    this->a_count.value = g_collada_elem_attribs.count;
  }
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_count,"count","uint",this,&this->a_count.value);
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"float_array","list_of_floats",parent,&this->_ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(polylist){
  element(mesh) * parent = (element(mesh)*) g_current_elem;
  element(polylist)** ptr = parent->ch_polylist;
  ptr = resize( ptr, ++parent->n_polylist * sizeof(element(polylist)*));
  parent->ch_polylist = ptr;
  element(polylist)* this = parent->ch_polylist[parent->n_polylist - 1];
  this = resize( this, sizeof(element(polylist)));

  this->p_mesh = parent;
  this->a_name.value = NULL;
  this->ch_input_local_offset = NULL;

  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_name;
  attribs[1] = (simple_element*) &this->a_count;


  if(g_collada_elem_attrib_states.count){
    this->a_count.value = g_collada_elem_attribs.count;
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_count,"count","uint",this,&this->a_count.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"polylist","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(vertices){
  element (mesh) * parent = (element(mesh)*) g_current_elem;
  element(vertices) * this = &parent->c_vertices;
  this->p_mesh = parent;
  
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->ch_input_local = NULL;
  
  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_id;
  attribs[1] = (simple_element*) &this->a_name;
  
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"vertices","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(source){
  element(mesh) * parent = (element(mesh)*) g_current_elem;
  element(source)** ptr = parent->ch_source;
  ptr = resize( ptr, ++parent->n_source * sizeof(element(source)*));
  parent->ch_source = ptr;
  element(source)* this = parent->ch_source[parent->n_source - 1];
  this = resize( this, sizeof(element(source)));

  this->p_mesh = parent;
  this->a_id.value = NULL;
  this->a_name.value = NULL;

  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_id;
  attribs[1] = (simple_element*) &this->a_name;
  
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"source","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(mesh){

  element(geometry) * parent = (element(geometry)*) g_current_elem;
  assert(parent);
  element(mesh)* this = &parent->c_mesh;
  this->p_geometry = parent;

  this->ch_source = NULL;
  this->ch_polylist = NULL;
  
  init_complex_element_base(this,"mesh","none",parent,NULL,NULL,NULL,0,0);
  g_current_elem = this;
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
  
  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->_ext.a_sid;
  
  if(g_collada_elem_attrib_states.sid){
    this->_ext.a_sid.value = resize( this->_ext.a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->_ext.a_sid.value,g_collada_elem_attribs.sid);
  }
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
  init_complex_element_base(this,"aspect_ratio","float",parent_ptr,&this->_ext._ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
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
  
  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->_ext.a_sid;

  if(g_collada_elem_attrib_states.sid){
    this->_ext.a_sid.value = resize( this->_ext.a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->_ext.a_sid.value,g_collada_elem_attribs.sid);
  }
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
  init_complex_element_base(this,"zfar","float",parent_ptr,&this->_ext._ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
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

  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->_ext.a_sid;
  
  if(g_collada_elem_attrib_states.sid){
    this->_ext.a_sid.value = resize( this->_ext.a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->_ext.a_sid.value,g_collada_elem_attribs.sid);
  }
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
  init_complex_element_base(this,"znear","float",parent_ptr,&this->_ext._ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(ymag){
  element(orthographic) * parent = (element(orthographic)*) g_current_elem;
  element(ymag)* this = &parent->c_ymag;
  this->p_orthographic = parent;
  this->_ext.a_sid.value = NULL;

  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->_ext.a_sid;
  
  if(g_collada_elem_attrib_states.sid){
    this->_ext.a_sid.value = resize( this->_ext.a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->_ext.a_sid.value,g_collada_elem_attribs.sid);
  }
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
  init_complex_element_base(this,"ymag","float",parent,&this->_ext._ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(xmag){
  element(orthographic) * parent = (element(orthographic)*) g_current_elem;
  element(xmag)* this = &parent->c_xmag;
  this->p_orthographic = parent;
  this->_ext.a_sid.value = NULL;
  
  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->_ext.a_sid;
  
  if(g_collada_elem_attrib_states.sid){
    this->_ext.a_sid.value = resize( this->_ext.a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->_ext.a_sid.value,g_collada_elem_attribs.sid);
  }
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
  init_complex_element_base(this,"xmag","float",parent,&this->_ext._ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(yfov){
  element(perspective) * parent = (element(perspective)*) g_current_elem;
  element(yfov)* this = &parent->c_yfov;
  this->p_perspective = parent;
  this->_ext.a_sid.value = NULL;
  
  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->_ext.a_sid;
  
  if(g_collada_elem_attrib_states.sid){
    this->_ext.a_sid.value = resize( this->_ext.a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->_ext.a_sid.value,g_collada_elem_attribs.sid);
  }
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
  init_complex_element_base(this,"yfov","float",parent,&this->_ext._ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(xfov){
  element(perspective) * parent = (element(perspective)*) g_current_elem;
  element(xfov)* this = &parent->c_xfov;
  this->p_perspective = parent;
  this->_ext.a_sid.value = NULL;
 
  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->_ext.a_sid;
  
  if(g_collada_elem_attrib_states.sid){
    this->_ext.a_sid.value = resize( this->_ext.a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->_ext.a_sid.value,g_collada_elem_attribs.sid);
  }
  init_simple_element_base(&this->_ext.a_sid,"sid","string",this,&this->_ext.a_sid.value);
  init_complex_element_base(this,"xfov","float",parent,&this->_ext._ext.value,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(orthographic){
  element(optics_technique_common) * parent = (element(optics_technique_common)*) g_current_elem;
  element(orthographic)* this = &parent->c_orthographic;
  this->p_optics_technique_common = parent;

  init_complex_element_base(this,"orthographic","none",parent,NULL,NULL,NULL,0,0);
  g_current_elem = this;
}

define_init_function(perspective){
  element(optics_technique_common) * parent = (element(optics_technique_common)*) g_current_elem;
  element(perspective)* this = &parent->c_perspective;

  init_complex_element_base(this,"perspective","none",parent,NULL,NULL,NULL,0,0);
  g_current_elem = this;
}

define_init_function(technique_common){
  simple_element * elem = (simple_element*) g_current_elem;
  assert(elem);
  if(strncmp(elem->name,"op",2) == 0 ){ // optics
    element(optics) * parent = (element(optics)*) g_current_elem;
    element(optics_technique_common)* this = &parent->c_optics_technique_common;
    this->p_optics = parent;
   
    init_complex_element_base(this,"technique_common","none",parent,NULL,NULL,NULL,0,0);
    g_current_elem = this;
  } else if (strncmp(elem->name,"so",2) == 0){ // source
    element(source) * parent = (element(source)*) g_current_elem;
    element(source_technique_common)* this = &parent->c_source_technique_common;
    this->p_source = parent;
    
    init_complex_element_base(this,"technique_common","none",parent,NULL,NULL,NULL,0,0);
    g_current_elem = this;
  }
}

define_init_function(optics){
  element(camera) * parent = (element(camera)*) g_current_elem;
  element(optics)* this = &parent->c_optics;
  this->p_camera = parent;

  init_complex_element_base(this,"optics","none",parent,NULL,NULL,NULL,0,0);
  g_current_elem = this;
}

define_init_function(instance_visual_scene){
  element(scene) * parent = (element(scene)*) g_current_elem;
  element(instance_visual_scene) * this = &parent->c_instance_visual_scene;
  this->p_scene = parent;
  this->a_sid.value = NULL;
  this->a_name.value = NULL;
  this->a_url.value = NULL;
  this->r_visual_scene = NULL;
 
  int n_attrib = 3;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_name;
  attribs[1] = (simple_element*) &this->a_sid;
  attribs[2] = (simple_element*) &this->a_url;
  
  if(g_collada_elem_attrib_states.sid){
    this->a_sid.value = resize( this->a_sid.value, strlen(g_collada_elem_attribs.sid)*sizeof(char)+sizeof(char));
    strcpy(this->a_sid.value,g_collada_elem_attribs.sid);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  if(g_collada_elem_attrib_states.url){
    this->a_url.value = resize( this->a_url.value, strlen(g_collada_elem_attribs.url)*sizeof(char)+sizeof(char));
    strcpy(this->a_url.value,g_collada_elem_attribs.url);
  }
  init_simple_element_base(&this->a_sid,"sid","string",this,&this->a_sid.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_simple_element_base(&this->a_url,"url","string",this,&this->a_url.value);
  init_complex_element_base(this,"instance_visual_scene","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(visual_scene){
  element(library_visual_scenes) * parent = (element(library_visual_scenes)*) g_current_elem;
  element(visual_scene)** ptr = parent->ch_visual_scene;
  ptr = resize( ptr, ++parent->n_visual_scene * sizeof(element(visual_scene)*));
  parent->ch_visual_scene = ptr;
  element(visual_scene)* this = parent->ch_visual_scene[parent->n_visual_scene - 1];
  this = resize( this, sizeof(element(visual_scene)));
  
  this->p_library_visual_scenes = parent;
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->ch_node = NULL;

  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_name;
  attribs[1] = (simple_element*) &this->a_id;
    
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"visual_scene","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(geometry){
  element(library_geometries) * parent = (element(library_geometries)*) g_current_elem;
  element(geometry)** ptr = parent->ch_geometry;
  ptr = resize( ptr, ++parent->n_geometry * sizeof(element(geometry)*));
  parent->ch_geometry = ptr;
  element(geometry)* this = parent->ch_geometry[parent->n_geometry - 1];
  this = resize( this, sizeof(element(geometry)));
  
  this->p_library_geometries = parent;
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  
  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_name;
  attribs[1] = (simple_element*) &this->a_id;
  
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"geometry","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(camera){
  element(library_cameras) * parent = (element(library_cameras)*) g_current_elem;
  element(camera)** ptr = parent->ch_camera;
  ptr = resize( ptr, ++parent->n_camera * sizeof(element(camera)*));
  parent->ch_camera = ptr;
  element(camera)* this = parent->ch_camera[parent->n_camera - 1];
  this = resize( this, sizeof(element(camera)));
  
  this->p_library_cameras = parent;	     
  this->a_id.value=NULL;
  this->a_name.value=NULL;
 
  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_name;
  attribs[1] = (simple_element*) &this->a_id;
  
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"camera","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(library_cameras){
  element(collada) * parent = (element(collada)*) g_current_elem;
  element(library_cameras)** ptr = parent->ch_library_cameras;
  ptr = resize( ptr, ++parent->n_library_cameras * sizeof(element(library_cameras)*));
  parent->ch_library_cameras = ptr;
  element(library_cameras)* this = parent->ch_library_cameras[parent->n_library_cameras - 1];
  this = resize( this, sizeof(element(library_cameras)));
  
  this->p_collada = parent;  
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->n_camera = 0;
  this->ch_camera = NULL;

  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_name;
  attribs[1] = (simple_element*) &this->a_id;
  
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"library_cameras","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(library_geometries){
  element(collada) * parent = (element(collada)*) g_current_elem;
  element(library_geometries)** ptr = parent->ch_library_geometries;
  ptr = resize( ptr, ++parent->n_library_geometries * sizeof(element(library_geometries)*));
  parent->ch_library_geometries = ptr;
  element(library_geometries)* this = parent->ch_library_geometries[parent->n_library_geometries - 1];
  this = resize( this, sizeof(element(library_geometries)));
  
  this->p_collada = parent;  
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->n_geometry = 0;
  this->ch_geometry = NULL;
 
  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_name;
  attribs[1] = (simple_element*) &this->a_id;
  
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"library_geometries","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(library_visual_scenes){
  element(collada) * parent = (element(collada)*) g_current_elem;
  element(library_visual_scenes)** ptr = parent->ch_library_visual_scenes;
  ptr = resize( ptr, ++parent->n_library_visual_scenes * sizeof(element(library_visual_scenes)*));
  parent->ch_library_visual_scenes = ptr;
  element(library_visual_scenes)* this = parent->ch_library_visual_scenes[parent->n_library_visual_scenes - 1];
  this = resize( this, sizeof(element(library_visual_scenes)));
  
  this->p_collada = parent;
  this->a_id.value = NULL;
  this->a_name.value = NULL;
  this->n_visual_scene = 0;
  this->ch_visual_scene = NULL;

  int n_attrib = 2;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_name;
  attribs[1] = (simple_element*) &this->a_id;
  
  if(g_collada_elem_attrib_states.id){
    this->a_id.value = resize( this->a_id.value, strlen(g_collada_elem_attribs.id)*sizeof(char)+sizeof(char));
    strcpy(this->a_id.value,g_collada_elem_attribs.id);
  }
  if(g_collada_elem_attrib_states.name){
    this->a_name.value = resize( this->a_name.value, strlen(g_collada_elem_attribs.name)*sizeof(char)+sizeof(char));
    strcpy(this->a_name.value,g_collada_elem_attribs.name);
  }
  init_simple_element_base(&this->a_id,"id","string",this,&this->a_id.value);
  init_simple_element_base(&this->a_name,"name","string",this,&this->a_name.value);
  init_complex_element_base(this,"library_visual_scenes","none",parent,NULL,NULL,attribs,0,n_attrib);
  g_current_elem = this;
}

define_init_function(scene){
  element(collada) * parent = (element(collada)*) g_current_elem;
  assert(parent);
  element(scene) * this = &parent->c_scene;
  this->p_collada = parent;
  
  init_complex_element_base(this,"scene","none",parent,NULL,NULL,NULL,0,0);
  g_current_elem = this;
}

define_init_function(collada){
  // bu satirda collada root oldugu icin cast yapmaya gerek yok
  element(collada)* this = g_collada;
  //
  this = resize( this, sizeof(element(collada)));
  this->a_version.value = NULL;
  this->ch_library_geometries = NULL;
  this->ch_library_cameras = NULL;
  this->ch_library_visual_scenes = NULL;

  int n_attrib = 1;
  simple_element** attribs = malloc(n_attrib * sizeof (simple_element*));
  attribs[0] = (simple_element*) &this->a_version;

  
  if(g_collada_elem_attrib_states.version){
    this->a_version.value = resize( this->a_version.value, strlen(g_collada_elem_attribs.version)*sizeof(char)+sizeof(char));
    strcpy(this->a_version.value,g_collada_elem_attribs.version);
  }
  init_simple_element_base(&this->a_version,"version","string",this,&this->a_version.value);
  init_complex_element_base(this,"COLLADA","none",NULL,NULL,NULL,attribs,0,n_attrib);
  g_collada = this;
  g_current_elem = this;
}


static void parse_attribs(void* userdata, const char** attr){
  size_t nattr=XML_GetSpecifiedAttributeCount((XML_Parser) userdata);
  size_t i=0;
  const char *ptr=NULL;
  // g_collada_elem_attribs = 0; burda sifirlama yapilmas lazim
  g_collada_elem_attrib_states = (struct collada_elem_attrib_states) {
    .id = 0,     
    .sid = 0,
    .name = 0,
    .type = 0,
    .version = 0,
    .url = 0,
    .stride = 0,
    .offset = 0,
    .set = 0,
    .meter = 0 
  };
  
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
      }
    }
  }

}

static void collada(elemend)(void *userdata,const char *elem){
  
  g_undefined_element_flag = 0;
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

static int* parse_list_of_ints(int* value_size,const XML_Char* str){
    char* end;
    const char* old_ptr = str;
    int i = 0;
    for (int f = strtol(str, &end,10); str != end; f = strtol(str, &end,10))
    {
        str = end;
        if (errno == ERANGE){
            printf("range error, got ");
            errno = 0;
        }
	i++;
    }
    int* arr = NULL;
    arr = resize(arr,i * sizeof(int));
    str = old_ptr;
    i = 0;
    for (int f = strtol(str, &end,10); str != end; f = strtol(str, &end,10))
    {
        str = end;
        if (errno == ERANGE){
            printf("range error, got ");
            errno = 0;
        }
	*(arr+i) = f;
	i++;
    }
    *value_size = i;
    return arr;
}

static unsigned int* parse_list_of_uints(int* value_size,const XML_Char* str){
    char *end;
    const char* old_ptr = str;
    int i = 0;
    for (unsigned int f = strtoul(str, &end,10); str != end; f = strtoul(str, &end,10))
    {
        str = end;
        if (errno == ERANGE){
            printf("range error, got ");
            errno = 0;
        }
	i++;
    }
    unsigned int* arr = NULL;
    arr = resize(arr,i * sizeof(unsigned int));
    str = old_ptr;
    i = 0;
    for (unsigned int f = strtoul(str, &end,10); str != end; f = strtoul(str, &end,10))
    {
        str = end;
        if (errno == ERANGE){
            printf("range error, got ");
            errno = 0;
        }
	*(arr+i) = f;
	i++;
    }
    *value_size = i;
    return arr;
}

static double* parse_list_of_floats(int* value_size,const XML_Char* str){
    char* end;
    const char* old_ptr = str;
    int i = 0;
    for (double f = strtod(str, &end); str != end; f = strtod(str, &end))
    {
        str = end;
        if (errno == ERANGE){
            printf("range error, got ");
            errno = 0;
        }
	i++;
    }
    double* arr = NULL;
    arr = resize(arr,i * sizeof(double));
    i=0;
    str = old_ptr;
    for (double f = strtod(str, &end); str != end; f = strtod(str, &end))
    {
        str = end;
        if (errno == ERANGE){
            printf("range error, got ");
            errno = 0;
        }
	*(arr+i) = f;
	i++;
    }
    *value_size = i;
    return arr;
}

static void parse_float4x4 (double arr[4][4] ,const XML_Char* str){
    char* end;
    int i = 0;
    int j = 0;
    
    for (double f = strtod(str, &end); str != end; f = strtod(str, &end))
    {
        str = end;
        if (errno == ERANGE){
            printf("range error, got ");
            errno = 0;
        }
	arr[i][j] = f;
	j++;
	if(j==4) {
	  j = 0;
	  i++;
	}
    }
}

static void collada(chardata)(void *userdata,const XML_Char *string,int len){
  int data_parsed = 0;
  
  if(g_current_depth==6){
    if(strncmp(g_current_elem_tag,"float_",6)==0){
      element(float_array)* this = g_current_elem;
      this->_ext.value = parse_list_of_floats(&this->_base.value_size,string);
      data_parsed = 1;
    }else if(strncmp(g_current_elem_tag,"int_",4)==0){
      element(int_array)* this = g_current_elem;
      this->_ext.value = parse_list_of_ints(&this->_base.value_size,string);
      data_parsed = 1;
    }else if(strcmp(g_current_elem_tag,"p")==0){
      element(p)* this = g_current_elem;
      this->_ext.value = parse_list_of_uints(&this->_base.value_size,string);
      data_parsed = 1;
    }else if(strncmp(g_current_elem_tag,"vc",2)==0){
      element(vcount)* this = g_current_elem;
      this->_ext.value = parse_list_of_uints(&this->_base.value_size,string);
      data_parsed = 1;
    }else{
      data_parsed = 0;
    }
  }else if(g_current_depth==5){
    if(strncmp(g_current_elem_tag,"matr",4)==0){
      element(matrix)* this = g_current_elem;
      this->_base.value_size = 16;
      parse_float4x4(this->_ext.value,string);
      data_parsed = 1;
    }else{
      data_parsed = 0;
    }
  }else if(g_current_depth==7){
    if(strncmp(g_current_elem_tag,"xf",2)==0){
      element(xfov)* this = g_current_elem;
      this->_ext._ext.value = strtod (string,NULL);
      data_parsed = 1;
    }else if(strncmp(g_current_elem_tag,"yf",2)==0){
      element(yfov)* this = g_current_elem;
      this->_ext._ext.value = strtod (string,NULL);
      data_parsed = 1;
    }else if(strncmp(g_current_elem_tag,"asp",3)==0){
      element(aspect_ratio)* this = g_current_elem;
      this->_ext._ext.value = strtod (string,NULL);
      data_parsed = 1;
    }else if(strncmp(g_current_elem_tag,"zn",2)==0){
      element(znear)* this = g_current_elem;
      this->_ext._ext.value = strtod (string,NULL);
      data_parsed = 1;
    }else if(strcmp(g_current_elem_tag,"zfar")==0){
      element(zfar)* this = g_current_elem;
      this->_ext._ext.value = strtod (string,NULL);
      data_parsed = 1;
    }else{
      data_parsed = 0;
    }
  }else if(g_current_depth==3){
    if(strncmp(g_current_elem_tag,"up",2)==0){
      data_parsed = 1;
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

}

static void collada(elemstart)(void *userdata,const char *elem,const char **attr){
  g_current_elem_tag = elem;
  g_undefined_element_flag = 0;
  // burdaki string karsilastirmasi icin hash fonksiyonu bulunacak  
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

static void collada(resolverefs)(void){

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

      g_current_depth=0;


      /* create parser and parse */
      p = XML_ParserCreate(NULL);
      assert(p);
      XML_UseParserAsHandlerArg(p);
      XML_SetElementHandler(p, element(elemstart),element(elemend));
      XML_SetCharacterDataHandler(p, element(chardata));
      
      if (!XML_Parse(p, string, fsize, -1)) {
	fprintf(stderr, "Parse error at line %lu:\n%s\n",XML_GetCurrentLineNumber(p),XML_ErrorString(XML_GetErrorCode(p)));    
      }else {
	/* resolve internal xml id references */

      }

      printf("\n\n\n\n");
      printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
      print_element((simple_element*)g_collada,0);
      
      /* cleaning */
      XML_ParserFree(p);
      p=NULL;
      free(string);
      string=NULL;

    }
  }
}
