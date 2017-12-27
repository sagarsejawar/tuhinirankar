/**
 * @file
 * Javascript file for metatag_upload module.
 */

(function ($) {
Drupal.behaviors.metatag_upload = {
        attach: function (context, settings) {
          $(':input[name="metatag_uploadcheck_uncheck_all"]').click(function() {
           if ($(this).is(':checked')) {
               metatag_upload_check_uncheck(true);
            } 
            else {
               metatag_upload_check_uncheck(false);
            }
          });
        }
};
})(jQuery);

/**
 * Function to check/uncheck all metatag checkboxes.
 * @param bool check
 */
function metatag_upload_check_uncheck(check) {
  jQuery('.metatag_upload_tag_types').find('input[type=checkbox]').each(function() {
    if (check) {
      jQuery(this).attr('checked', true);
    }
    else {
      jQuery(this).attr('checked', false);
    }
  });
}