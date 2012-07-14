<?php
header("Cache-Control: no-cache, must-revalidate");

include 'intercom_db.php';

$key = secure_sql($_REQUEST['key']);
$value = secure_sql($_REQUEST['value']);
if($key && $value)
{
    $db = intc_create_db();
    if(intc_set_option($db,$key,$value))
    {
	print(intc_get_option($db,$key));
    }
    else
	print("ERROR");
}
else
    print("ERROR");
?>