<?php
$hostname = exec('hostname');
define('hostname',$hostname);
define('messages_log','/var/log/messages');
define('http_user','root');
define('http_passwd_file','/opt/.webauth');
include 'custom_config.php';
?>