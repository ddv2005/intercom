<?php
include 'intercom_db.php';

require_once ABSPATH."jqGrid.php";
// include the driver class
require_once ABSPATH."jqGridPdo.php";
// Connection to the server
$conn = new PDO(DB_DSN,DB_USER,DB_PASSWORD);

// Create the jqGrid instance
$grid = new jqGridRender($conn);
// Write the SQL Query
$grid->SelectCommand = 'SELECT * FROM tblOptions';
$grid->table = 'tblOptions'; 
// set the ouput format to json
$grid->dataType = 'json';
// Let the grid create the model
$grid->setColModel();
// Set the url from where we obtain the data
$grid->setUrl('options.php');
$grid->setPrimaryKeyId("id");
// Set in grid options scroll to 1
$grid->setGridOptions(array("hoverrows"=>true, "scroll"=>1,    "rownumbers"=>true,
		"rownumWidth"=>35, "rowNum"=>100,"sortname"=>"id","height"=>200));
// Change some property of the field(s)
$grid->setColProperty("id", array("label"=>"ID","hidden"=>true,"editable"=>false,"width"=>20));
$grid->setColProperty("name", array("label"=>"Name","width"=>100));
$grid->setColProperty("value", array("label"=>"Value","width"=>80));

$grid->navigator = true;
$grid->setNavOptions('navigator', array("add"=>true,"edit"=>true,"excel"=>false));
// and just enable the inline
$grid->inlineNav = true;
$grid->renderGrid('#grid','#pager',true, null, null, true,true);
$conn = null; 
?>