<?php
include 'intercom_db.php';

$database = intc_create_db();

$query = "SELECT * FROM tblOptions";
if($result = $database->query($query))
{
  while($row = $result->fetchArray())
  {
    print("{$row['name']}=" .
          "{$row['value']}<br />");
  }
}
else
{
  die($error);
}
?>