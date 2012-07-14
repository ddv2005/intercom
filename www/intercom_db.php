<?php
include 'config.php';


function secure_sql($value)
{
    $value = htmlspecialchars(stripslashes($value));
    $value = str_ireplace("script", "blocked",$value);
    $value = SQLite3::escapeString($value);
    return $value;
}

function intc_create_db()
{
    global $db_path;
    $database = new SQLite3($db_path);
    if (!$database) die ($error);
    return $database;
}

function intc_set_option($db,$key,$value)
{
    if($db->querySingle("SELECT COUNT(*) FROM tblOptions WHERE name='$key'")>0)
    {
	return $db->exec("UPDATE tblOptions SET value='$value' WHERE name='$key'");
    }
    else
    {
	return $db->exec("INSERT INTO tblOptions(name,value) VALUES('$key','$value')");
    }
}

function intc_get_option($db,$key)
{
     return $db->querySingle("SELECT value FROM tblOptions WHERE name='$key'");
}

?>