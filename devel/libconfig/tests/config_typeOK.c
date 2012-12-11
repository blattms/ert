/*
   Copyright (C) 2012  Statoil ASA, Norway. 
    
   The file 'config_typeOK.c' is part of ERT - Ensemble based Reservoir Tool. 
    
   ERT is free software: you can redistribute it and/or modify 
   it under the terms of the GNU General Public License as published by 
   the Free Software Foundation, either version 3 of the License, or 
   (at your option) any later version. 
    
   ERT is distributed in the hope that it will be useful, but WITHOUT ANY 
   WARRANTY; without even the implied warranty of MERCHANTABILITY or 
   FITNESS FOR A PARTICULAR PURPOSE.   
    
   See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
   for more details. 
*/
#include <stdlib.h>
#include <stdbool.h>

#include <config.h>


void error(char * msg) {
  fprintf(stderr , msg);
  exit(1);
}


int main(int argc , char ** argv) {
  const char * config_file = argv[1];
  config_type * config = config_alloc();
  bool OK;
  {
    config_schema_item_type * item  = config_add_schema_item(config , "TYPE_KEY" , false );
    config_schema_item_set_argc_minmax( item , 4 , 4 , 4 , (const config_item_types [4]) {CONFIG_INT , CONFIG_FLOAT , CONFIG_BOOLEAN , CONFIG_STRING });
      
    item = config_add_schema_item( config , "SHORT_KEY" , false );
    config_schema_item_set_argc_minmax( item , 1 , 1 , 0 , NULL );
    
    item = config_add_schema_item( config , "LONG_KEY" , false );
    config_schema_item_set_argc_minmax( item , 3 , CONFIG_DEFAULT_ARG_MAX , 0 , NULL );
  }
  OK = config_parse(config , config_file , "--" , NULL , NULL , false , true );
  
  if (OK) {
    
  } else error("Parse error\n");
  
  exit(0);
}
