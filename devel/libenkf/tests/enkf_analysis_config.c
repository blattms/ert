/*
   Copyright (C) 2013  Statoil ASA, Norway. 
    
   The file 'enkf_analysis_config.c' is part of ERT - Ensemble based Reservoir Tool. 
    
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
#include <stdio.h>
#include <unistd.h>

#include <ert/util/test_util.h>
#include <ert/util/util.h>
#include <ert/util/rng.h>

#include <ert/enkf/analysis_config.h>


analysis_config_type * create_analysis_config() {
  rng_type * rng = rng_alloc( MZRAN , INIT_DEFAULT );
  analysis_config_type * ac = analysis_config_alloc( rng );
  return ac;
}


void test_create() {
  analysis_config_type * ac = create_analysis_config( );
  test_assert_true( analysis_config_is_instance( ac ) );
  analysis_config_free( ac );
}



void test_min_realisations( ) {
  analysis_config_type * ac = create_analysis_config( );
  test_assert_int_equal( 0 , analysis_config_get_min_realisations( ac ) );
  analysis_config_set_min_realisations( ac , 26 );
  test_assert_int_equal( 26 , analysis_config_get_min_realisations( ac ) );
  analysis_config_free( ac );
}


void test_continue( ) {
  analysis_config_type * ac = create_analysis_config( );

  test_assert_true( analysis_config_have_enough_realisations( ac , 10 ));
  test_assert_false( analysis_config_have_enough_realisations( ac , 0 ));

  analysis_config_set_min_realisations( ac , 5 );
  test_assert_true( analysis_config_have_enough_realisations( ac , 10 ));

  analysis_config_set_min_realisations( ac , 15 );
  test_assert_false( analysis_config_have_enough_realisations( ac , 10 ));

  analysis_config_set_min_realisations( ac , 10 );
  test_assert_true( analysis_config_have_enough_realisations( ac , 10 ));

  analysis_config_set_min_realisations( ac , 0 );
  test_assert_true( analysis_config_have_enough_realisations( ac , 10 ));

  analysis_config_free( ac );
}


void test_current_module_options() {
  analysis_config_type * ac = create_analysis_config( );
  test_assert_NULL( analysis_config_get_active_module( ac ));
  analysis_config_load_internal_module(ac , "STD_ENKF" , "std_enkf_symbol_table");

  test_assert_false( analysis_config_get_module_option( ac , ANALYSIS_SCALE_DATA));
  test_assert_true(analysis_config_select_module(ac , "STD_ENKF"));
  test_assert_false( analysis_config_select_module(ac , "DOES_NOT_EXIST"));

  test_assert_true( analysis_module_is_instance( analysis_config_get_active_module( ac )));
  test_assert_true( analysis_config_get_module_option( ac , ANALYSIS_SCALE_DATA));
  test_assert_false( analysis_config_get_module_option( ac , ANALYSIS_ITERABLE));
  analysis_config_free( ac );
}

void test_stop_long_running( ) {
  analysis_config_type * ac = create_analysis_config( );
  test_assert_bool_equal( false , analysis_config_get_stop_long_running( ac ) );
  analysis_config_set_stop_long_running( ac , true );
  test_assert_bool_equal( true , analysis_config_get_stop_long_running( ac ) );
  analysis_config_free( ac );
}

int main(int argc , char ** argv) {  
  test_create();
  test_min_realisations();
  test_continue();
  test_current_module_options();
  test_stop_long_running();
  exit(0);
}

