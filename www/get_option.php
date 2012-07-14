<?php
header("Cache-Control: no-cache, must-revalidate");

include 'intercom_db.php';

$key = secure_sql($_REQUEST['key']);
if($key)
{
    $db = intc_create_db();
    print(intc_get_option($db,$key));
}
else
    print("ERROR");
?>