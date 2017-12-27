<?php

/**
 * @file
 * Inc file for metatag_upload module.
 */

/**
 * Form handler for uploading meta tags by csv.
 * @return string 
 */
function _metatag_upload() {
  $form = array();
  $form['metatags_upload_warning_message'] = array(
      '#markup' => '<div class="warning-message-metatag-upload"><h1>' . t('WARNING') . '</h1>' . t('Please take the backup of database first and disable the Purge and Cache Expiration module from the module listing before uploading metatags.'). '</div>',
  );

  $form['metatags'] = array(
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

  $form['meta_tag_fieldset'] = array(
      '#type' => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#title' => t('Select Meta Tags'),
  );

  $form['meta_tag_fieldset']['metatag_uploadcheck_uncheck_all'] = array(
      '#type' => 'checkbox',
      '#title' => '<strong>' . t('Select All') . '</strong>',
  );

  $info = metatag_get_info(); //Get all meta tags
  $options = array();
  foreach ($info['tags'] as $key => $val) { //Iterate all metatags to make key=>value pair array.
    $options[$key] = $info['tags'][$key]['label'];
  }
  $form['meta_tag_fieldset']['meta_tag_types'] = array(
      '#type' => 'checkboxes',
      '#prefix' => '<div class=\'metatag_upload_tag_types\'>',
      '#suffix' => '</div>',
      '#options' => $options,
      '#required' => TRUE,
      '#description' => '<strong>' . t('Select the meta tags which you want to upload.') . '</strong>',
  );

  $form['meta_tag_format_example_fieldset'] = array(
      '#type' => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#title' => t('Example of tags format used in csv'),
  );

  $markup = '';
  foreach ($options as $key => $val) {
    $markup .= '<div class=\'meta-tag-csv-head-example\'><strong>' . ucwords($val) . '</strong> => ' . $key . '</div>';
  }
  $form['meta_tag_format_example_fieldset']['meta_tag_csv_fields_example'] = array(
      '#markup' => $markup,
      '#prefix' => '<div class=\'metatag_upload_tag_types\'>',
      '#suffix' => '</div>',
  );

  $form['metatags_upload_submit'] = array(
      '#value' => t('Upload'),
      '#type' => 'submit',
  );

  $form['metatag_upload_suggestion'] = array(
      '#markup' => '<strong><div>' . t('Note:- Please check there be atleast one entry for path based metatgs for metatags_quick module.') . '</div><div>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; Also enable the path based setting for Metatags_quick module.</div></strong>',
  );

  $form['#attached']['js'] = array(
      drupal_get_path('module', 'metatag_upload') . '/js_css/metatag_upload.js',
  );
  $form['#attached']['css'] = array(
      drupal_get_path('module', 'metatag_upload') . '/js_css/metatag_upload.css',
  );
  return $form;
}

/**
 * Validation handler for _metatag_upload form.
 * @param array $form
 * @param array $form_state 
 */
function _metatag_upload_validate($form, &$form_state) {
  $meta_tag_types = array_filter($form_state['values']['meta_tag_types']); //Get selected meta tags.
  if (is_array($meta_tag_types) && empty($meta_tag_types)) {
    form_set_error('meta_tag_fieldset', t('Please select at least one metatag type.'));
  }
}

/**
 * Submit handler for _metatag_upload form.
 * @param array $form
 * @param array $form_state 
 */
function _metatag_upload_submit($form, &$form_state) {
  $meta_tag_types = array_filter($form_state['values']['meta_tag_types']); //Get selected meta tags.
  //Read meta tags from csv file and store in an array.
  $data = _metatg_upload_read_csv(file_load($form_state['values']['metatags']));
  $batch = array(
      'title' => t('Importing metatags...'),
      'error_message' => t('An error occurred during processing'),
      'init_message' => t('Importing'),
      'operations' => array(),
      'file' => drupal_get_path('module', 'metatag_upload') . '/include/metatag_upload.admin.inc',
      'finished' => '_metatag_upload_batch_finished',
  );
  $header = $data[0]; //Header fields from csv.
  unset($data[0]); //Remove header from data of csv.
  foreach ($data as $row) {
    //For testing purpose without batch, uncomment below function and comment batch_set().
    //_metatag_upload_batch_processing($row, $header, $meta_tag_types, $context);
    //Setting the batch operations for each csv row.
    $batch['operations'][] = array('_metatag_upload_batch_processing', array($row, $header, $meta_tag_types));
  }
  //Batch Processing starts here.
  batch_set($batch);
}

/**
 * Batch processing callback to upload/import meta tags from CSV file for each row.
 * @param array $row
 * @param array $header
 * @param array $meta_tag_types
 * @param array $context 
 */
function _metatag_upload_batch_processing($row, $header, $meta_tag_types, &$context) {
  $url = $row[0]; //It is assumed that always at 0th index(first column), url will be provided.
  $result = _metatag_upload_get_path($url);
  if ($result != FALSE) { //If path exists (an entity).
    $path_info = _metatag_upload_entity_type($result);
    $type = $path_info['type'];
    $id = $path_info['id'];
    _metatag_upload_entity_based_processing($type, $id, $row, $header, $meta_tag_types, $context);
  }else {  //If path not exists (not an entity).
    $type = 'path_based';
    if (!drupal_valid_path($url)) {
      $type = 'invalid_path';
      $context['results']['invalid_path'][] = t('Path \'@path\' does not exist in the system', array('@path' => $url));
    } else {
      _metatag_upload_path_based_processing($url, $row, $header, $meta_tag_types, $context);
    }
  }
}

/**
 * Batch process finsh function for event upload
 * @param boolean $success
 * @param array $results
 * @param array $operations 
 */
function _metatag_upload_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('File uploaded successfully');
    $message .= '<br>';
    foreach ($results as $type) {
      foreach ($type as $msg) {
        $message .= $msg . '<br>';
      }
    }
    if (count($results) <= 0) {
      $message .= t('There is nothing to update.');
    }
  } else {
    $message = t('File uploaded with errors');
  }
  cache_clear_all(); //Clear all the cache.
  drupal_set_message($message);
}

/**
 * Function to read csv file based on file uri and 
 * delete the uploaded file as we don't need this.
 * @param object $file
 */
function _metatg_upload_read_csv($file) {
  //Get the complete file path (with file name).
  $filepath = drupal_realpath($file->uri);
  $data = array();
  $handle = fopen($filepath, "r");
  //Read the csv file to get array of data.
  while ($row = fgetcsv($handle)) {
    $data[] = $row;
  }
  fclose($handle);
  //Delete the file as we don't need this (This will remove the file from server as well as from database).
  file_delete($file);
  return $data;
}

/**
 * Method to add meta tags to a given entity.
 * @param String $type
 * @param integer $id  Entity Id.
 * @param array $row   Row of Metatags.
 * @param array $header  Header containing Metatag types.
 * @param array $meta_tag_types  Types of metatags that are checked while submitting the form.
 * @param array $context  Context object called by reference to be used for batch finished messages.
 */
function _metatag_upload_entity_based_processing($type, $id, $row, $header, $meta_tag_types, &$context) {

  unset($row[0]); //Remove the url part from row.
  unset($header[0]); //Remove the url column name from header.
  $function_load = strtolower($type) == 'taxonomy' ? 'taxonomy_term_load' : strtolower($type) . '_load';
  $skip_path = _metatags_upload_check_url_path($id);
  if($skip_path != 'skip') {
  $entity = $function_load($id); //Load the entity.
  if (!$entity) { //If entity with given id does not exist.
    $path = $type == 'taxonomy' ? 'taxonomy/term/' . $id : $type . '/' . $id;
    $context['results']['invalid_path'][] = t('Path \'@path\' does not exist in the system', array('@path' => $path));
  } else {
    $meta_tags = $entity->metatags;  //Get the metatags of entity.
    if (!isset($meta_tags['und']) || empty($meta_tags['und'])) {
      $meta_tags = array('und' => array());  //If get no meta tags (blank for entity).
    }
    foreach ($header as $key => $val) {
      if (in_array($val, $meta_tag_types)) { //Only those type of metatags whose checkboxes are checked in csv upload form.
        if (isset($row[$key]) && !empty($row[$key])) {  //If meta supplied is in csv file and not empty.
          if (isset($meta_tags['und'][$val]) && $meta_tags['und'][$val]['value'] != $row[$key]) {  //Sets output message if tag updated.
            $old_val = $meta_tags['und'][$val]['value'];
            if ($val == 'robots') {  //proper format array for robot tags.
              $robots = _metatag_upload_robot_processing($row[$key]);
              if (!empty($robots)) {  //If array is not empty.
                $meta_tags['und'][$val]['value'] = $robots;
              }
            } else {
              $meta_tags['und'][$val]['value'] = $row[$key];  //Updating meta tag.
              $context['results'][$type][] = t('Meta Tag \'@tag\' is updated from \'@old_val\' to \'@new_val\' for \'@entity\' @id', array('@tag' => ucwords($val), '@old_val' => $old_val, '@new_val' => $row[$key], '@entity' => $type, '@id' => $id));
            }
          } elseif (isset($meta_tags['und'][$val]) && $meta_tags['und'][$val]['value'] == $row[$key]) {
            //If value exists and same, then do nothing.
            continue;
          } else { //If added new meta tag to entity.
            if ($val == 'robots') {  //proper format array for robot tags.
              $robots = _metatag_upload_robot_processing($row[$key]);
              if (!empty($robots)) {
                $meta_tags['und'][$val] = array('value' => $robots);  //Add the robot meta tag.
              }
            } else {
              $meta_tags['und'][$val] = array('value' => $row[$key]);  //Add the meta tag.
              $context['results'][$type][] = t('Meta Tag \'@tag\' is added with value \'@val\' for \'@entity\' @id', array('@tag' => ucwords($val), '@val' => $row[$key], '@entity' => $type, '@id' => $id));
            }
          }
        }
      }
    }

    if (module_exists('page_title') && isset($meta_tags['und']['title']['value']) && !empty($meta_tags['und']['title']['value'])) {  //If page title module is installed and enabled and title metatag is set and not empty.
      $entity->page_title = check_plain($meta_tags['und']['title']['value']);  //override the page title of page_title module.
    }
    $entity->metatags = $meta_tags['und'];
    $function_save = $type == 'taxonomy' ? 'taxonomy_term_save' : $type . '_save';
    $function_save($entity); //Save the entity.
  }
  } else {
    $path = $type == 'taxonomy' ? 'taxonomy/term/' . $id : $type . '/' . $id;
    $context['results']['invalid_path'][] = t('Path \'@path\' has been skipped by the system', array('@path' => $path));
  }
}

/**
 * Method to return the actual path (source) for a given path otherwise false.
 * @param String $path
 * @return boolean 
 */
function _metatag_upload_get_path($path) {
  global $base_url;
  if (valid_url($path, TRUE)) { //If full absolute url.
    $path = substr($path, strlen($base_url) + 1); //Get the path after removing baseurl.
  }
  if(empty($path)) {
    $path = variable_get('site_frontpage', 'node');
  }
  $entity_types = array('node', 'user', 'taxonomy', 'comment');
  $entity = explode('/', $path);
  if (in_array(strtolower($entity[0]), $entity_types)) { //If supplied path is actual path, then return as it.
    return $path;
  }
  $path = urldecode($path);
  $source = drupal_lookup_path('source', $path);  //Return the actual url of given alias.
  return $source;
}

/**
 * Method to return the entity type and id by given path.
 * @param type $path
 * @return type 
 */
function _metatag_upload_entity_type($path) {
  $result = explode('/', $path);
  $type = strtolower($result[0]);
  $id = ($type == 'user' || $type == 'node' || $type == 'comment') ? $result[1] : $result[2];  //For only taxonomy page we get id from arg(2).
  return array('type' => $type, 'id' => $id);
}

/**
 * Upload Meta tags for the paths (views).
 * @param type $path  Path to which metatags to be added.
 * @param type $row   Row of metatags to be added.
 * @param type $header  Header containing metatag types.
 * @param type $meta_tag_types  Type of metatags that are checked while submitting the form.
 * @param type $context Context object called by reference used in batch finished for messages.
 */
function _metatag_upload_path_based_processing($path, $row, $header, $meta_tag_types, &$context) {
  global $base_url;
  $controller = new DrupalDefaultEntityController('metatags_path_based');
  if (valid_url($path, TRUE)) { //If full absolute url.
       $path = substr($path, strlen($base_url) + 1); //Get the path after removing baseurl.
	   }
  $lang = 'en';
  $entity_id = db_select('metatags_quick_path_based', 'm')
          ->fields('m', array('id'))
          ->condition('lang', $lang)
          ->condition('path', $path)
          ->execute()
          ->fetchField();
  if (!$entity_id) {   //If path not exists in quick_metatag db, then create the path entry in quick_metatag db.    
    $entity_id = db_insert('metatags_quick_path_based')
            ->fields(array('lang' => $lang, 'path' => $path))
            ->execute();
  }

  $entities = $controller->load(array($entity_id));
  $entity = $entities[$entity_id];
  unset($row[0]); //Remove path url.
  unset($header[0]); //Remove path heading.
  $temp_entity = (array) $entity;  //Converting object to array.
  module_load_include('inc', 'metatags_quick', 'known_tags');  //Load the file so that get supported metatags.
  $tag_array = array_intersect(array_keys(_metatags_quick_known_fields()), $meta_tag_types);  //Get tag array list which supported by metatags_quick and checked while submitting form.
  foreach ($row as $key => $val) {
    if (!empty($val) && in_array($header[$key], $tag_array)) {  //Only those metatags which are checked while submitting the form and not blank.
      $tag = 'meta_' . $header[$key];
      //If metatag value exists.
      if (isset($temp_entity[$tag]['und']) && $temp_entity[$tag]['und'][0]['meta_name'] == $header[$key] && $temp_entity[$tag]['und'][0]['metatags_quick'] != $val) {
        $old_val = $temp_entity[$tag]['und'][0]['metatags_quick'];
        $temp_entity[$tag]['und'][0]['metatags_quick'] = $val;  //Updating value.
        $context['results']['path_based'][] = t('Metatag \'@tag\' is updated from \'@old_val\' to \'@new_val\' for path \'@path\'', array('@tag' => $header[$key], '@old_val' => $old_val, '@new_val' => $val, '@path' => $path));
      } elseif (isset($temp_entity[$tag]['und']) && $temp_entity[$tag]['und'][0]['meta_name'] == $header[$key] && $temp_entity[$tag]['und'][0]['metatags_quick'] == $val) { //If value exists and same, then do nothing.
        continue;
      } else {
        //Add metatags.
        $temp_entity[$tag] = array('und' => array(array('metatags_quick' => $val, 'meta_name' => $header[$key])));
        $context['results']['path_based'][] = t('Metatag \'@tag\' is added with value \'@val\' for path \'@path\'', array('@tag' => $header[$key], '@val' => $val, '@path' => $path));
      }
    }
  }
  field_attach_update('metatags_path_based', (object) $temp_entity); //Save the metatags for path.
}

/**
 * Method to arrange robot metatags in proper format.
 * @param String $robots
 * @return array
 */
function _metatag_upload_robot_processing($robots) {
  $result = array();
  $robot_array = explode(',', $robots);
  foreach ($robot_array as $key => $val) {
    $result[$val] = $val;
  }
  return $result;
}

/**
 * Method to check if the upload path needs to be skipped.
 * @param type $id
 * @return string 
 */
function _metatags_upload_check_url_path($id) {
  $path_types = array('password', 'login', 'logout', 'passwords');
  if (in_array(strtolower($id), $path_types) || empty($id)) {
    return 'skip';
  } else {
    return $id;
  }
}