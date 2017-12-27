<?php

/**
 * @file
 * Admin settings file for metatag_download module. 
 */

/**
 * Form Handler for downloading metatags by csv.
 * @return type 
 */
function _metatag_download() {
  $form = array();
  drupal_set_title(t('Download Meta Tags'));
  $form['metatags_download'] = array(
      '#type' => 'managed_file',
      '#title' => t('Select File'),
      '#required' => TRUE,
      '#description' => t('Allowed Extensions: csv'),
      '#upload_location' => 'public://metatag_upload/',
      '#upload_validators' => array(
          'file_validate_extensions' => array('csv'), //File type validation.
          'file_validate_size' => array(1 * 1024 * 1024), //File size 1 MB validation (Not working).
      ),
  );

  $form['metatags_download_submit'] = array(
      '#value' => t('Download'),
      '#type' => 'submit',
  );
  $form['metatags_download_suggestion'] = array(
      '#markup' => '<div><strong>' . t('Note:- Please refresh the page once you download the csv.') . '</strong></div>',
  );
  return $form;
}

/**
 * Submit handler for _metatag_download form.
 * @param type $form
 * @param type $form_state 
 */
function _metatag_download_submit($form, &$form_state) {
  module_load_include('inc', 'metatag_upload', '/include/metatag_upload.admin');
  $data = _metatg_upload_read_csv(file_load($form_state['values']['metatags_download'])); //Read the csv.
  $csv = array();
  array_shift($data); //Remove the heading of column.
  foreach ($data as $url) {
    $result = _metatag_upload_get_path($url[0]);
    
    if ($result != FALSE) { //If path exists (an entity).
      $path_info = _metatag_upload_entity_type($result);
      $function_load = strtolower($path_info['type']) == 'taxonomy' ? 'taxonomy_term_load' : strtolower($path_info['type']) . '_load';
      $entity = $function_load($path_info['id']); //Load the entity.
      if (!$entity) { //If entity not exist by id.
        continue;
      } else {
        $default_tags = array();
        $entity_instance = strtolower($path_info['type']) == 'node' ? 'node:' . $entity->type : strtolower($path_info['type']);
        $meta_tags = metatag_download_entity_tags_val($entity_instance, $entity, $path_info['type']);
        if(module_exists('page_title')){
        $pagetitle = get_title_by_pagetitle($result);
        $meta_tags['page-title-by-pagetitle']['value'] = $pagetitle;
        }
        $csv[$url[0]] = $meta_tags;
      }
      
      
    } else {  //For path based.
      if (!drupal_valid_path($url[0]) && !drupal_valid_path(drupal_lookup_path('source', $url[0]))) {  //If path (url) not exists in system.
        continue;
      }
      $path_tags = _metatag_download_path_based($url[0]);
      if(module_exists('page_title')){
      $path_tags['page-title-by-pagetitle']['value'] = 'Not a Node';
      }
      if (is_array($path_tags) && !empty($path_tags)) {
        $csv[$url[0]] = $path_tags;
      }
    }
  }
  _metatag_download_csv_downloadable_array($csv);
  drupal_exit();  //Stops the redirection so that file can be downloaded.
}

/**
 * Method to return the metatags for a given path.
 * @global type $base_url
 * @param type $path
 * @return type 
 */
function _metatag_download_path_based($path) {
  global $base_url;
  if (valid_url($path, TRUE)) { //If full absolute url.
    $path = substr($path, strlen($base_url) + 1); //Get the path after removing baseurl.
  }
  $metatags = array();
  $controller = new DrupalDefaultEntityController('metatags_path_based');
  $lang = 'en';
  $entity_id = db_select('metatags_quick_path_based', 'm')
          ->fields('m', array('id'))
          ->condition('lang', $lang)
          ->condition('path', $path)
          ->execute()
          ->fetchField();
  if (!$entity_id) { //If url not exists in metatags_quick table.
    $metatags = _metatag_download_get_default_path_based();
  } else {
    $entities = $controller->load(array($entity_id));
    $metatags = _metatag_download_get_path_based_meta((array)$entities[$entity_id]);
  }
  return $metatags;
}

/**
 * Method to generate csv file with array and helps to download it.
 * @param array $data 
 */
function _metatag_download_csv_downloadable_array($data) {  
  $output_final = array();
  $header = array();
  $info = metatag_get_info();
  foreach ($info['tags'] as $key => $val) { //Iterate all metatags to make key=>value pair array.
    $header[] = $key;    
  }
  $header[] = 'page-title-by-pagetitle';
  array_unshift($header, 'Path');
  foreach ($data as $key => $val) {  //Generate final array by replacing token values if exists.
    $output_temp = array();
    foreach ($val as $k1 => $v1) {
	if($k1 == 'robots') {
	  $v1 = _metatags_download_robots_check($v1);
	}
      $tag_key = array_search($k1, $header);
      if ($tag_key) { //If key exists.
        $output_temp[$tag_key] = $v1['value'];
      }
    }
    $output_temp[0] = $key;
    $output_final[] = $output_temp;
  }
  array_unshift($output_final, $header); //Add heading fields in final array.
  $header_count = count($header);  //Count header fields.
  $new_arr = array();
  foreach ($output_final as $key1 => $val1) {  //Rearrange final array with index so that can be write properly in csv.
    $temp_arr = array();
    for ($i = 0; $i < $header_count; $i++) {
      if (isset($val1[$i])) {
        $temp_arr[$i] = $val1[$i];
      } else {
        $temp_arr[$i] = ''; //If not contains meta tag than assign blank for that meta tag type.
      }
    }
    ksort($temp_arr);  //Sort the array.
    $new_arr[] = $temp_arr;
  }
  $output_final = $new_arr;
  drupal_add_http_header('Content-Type', 'text/csv');
  drupal_add_http_header('Content-Disposition', 'attachment;filename=Meta_Tag_File.csv');
  $fp = fopen('php://output', 'w');
  foreach ($output_final as $tag) {
    fputcsv($fp, $tag);
  }
  fclose($fp);
}

/**
 * Method to get metatags from Default metatag configuration of metatag module.
 * @param type $instance
 * @return array
 */
function _metatag_download_get_default_path_based($instance = 'global') {
  $default_tags = metatag_config_load_with_defaults($instance);
  $tags = $default_tags;
  foreach ($default_tags as $key => $val) {
    $tags[$key]['value'] = $key == 'robots' ? implode(',', array_filter($val['value'])) : token_replace($val['value']);
    if ($key != 'robots' && empty($val['value']))unset($tags[$key]);  //Remove metatags which are empty.
  }
  return $tags;
}

/**
 * Method to get downloadable metatags for entity type.
 * @param type $instance
 * @param type $entity
 * @param type $entity_type
 * @return type 
 */
function metatag_download_entity_tags_val($instance, $entity, $entity_type) {
  $entity_tags = $entity->metatags;
  $default_tags = metatag_config_load_with_defaults($instance);
  if (isset($entity_tags['und']) || isset($entity_tags['en'])) {  //Metatags exists for entity.
    if(isset($entity_tags['und'])) {
       $tags = $entity_tags['und'];
    } elseif(isset($entity_tags['en'])) {
        $tags = $entity_tags['en'];
    }	 

    foreach ($tags as $tag_name => $tag_val) {
      if ($tag_name == 'robots') { //For robots tags.
        $tags[$tag_name]['value'] = implode(',', array_filter($tags[$tag_name]['value']));
      }
      else { //If page_title is set from the page_title module.
        if (isset($entity->page_title) && !empty($entity->page_title) && $tag_name = 'title') {
          $tags[$tag_name]['value'] = token_replace($entity->page_title, array($entity_type => $entity));
        }
        else {
          $tags[$tag_name]['value'] = !empty($tag_val['value']) ? token_replace($tag_val['value'], array($entity_type => $entity)) : (isset($default_tags[$tag_name]) ? token_replace($default_tags[$tag_name]['value'], array($entity_type => $entity)) : ""); //replace token and merge default and entity metatags.
        }
      }
    }
	//if(!isset($tags['robots'])) {
	//  $tags['robots'] = $default_tags['robots'];
	//}
    return array_merge($default_tags, $tags);
  }
  else { //Return default tags only if entity metatags not set.
    return $default_tags;
  }
}

/**
 * Method to get the metatags for path from metatags_quick module.
 * @param type $entity
 * @return array
 */
function _metatag_download_get_path_based_meta($entity) {
  $tags = array();
  foreach ($entity as $key => $val) {
    if (is_array($val) && isset($entity[$key]['und']) && $key != 'rdf_mapping') {
      $tags[$entity[$key]['und'][0]['meta_name']] = array('value' => token_replace($entity[$key]['und'][0]['metatags_quick']));
    }
  }
  return array_merge(_metatag_download_get_default_path_based(), $tags);
}

/**
 * Method to arrange robot metatags in proper format for csv.
 * @param array $data
 * @return array
 */
function _metatags_download_robots_check($data) {
  $result = '';
  if(is_array($data['value'])) { //Check if the robots values is an array or String
    foreach($data['value'] as $keys => $values) {
	  if(isset($values) && !empty($values)) { //Generate a string if the values of the keys are not empty 
	    $result = $result + $values + ',';
	  } else {
	    $result = '';
	  }
	 }
	 $data['value'] = $result;
	 return $data; 
  } else {
    return $data;
  }
}

function get_title_by_pagetitle($url){
    if ($url != '') {
        $tmp = explode("/", $url);
        if ($tmp[0] = 'node') {
            $result = db_select('page_title', 'pt')
                    ->fields("pt", array("page_title"))
                    ->condition('id', $tmp[1], '=')
                    ->execute()
                    ->fetchField();
            return $result;
        } else {
            return '';
        }
    } else {
        return '';
    }
}