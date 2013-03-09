    <div id="sidebar">
      <h1>Intercom Pages</h1>
      <div id="menu">
        <a  <?php  if(preg_match("/index.php/",$_SERVER['REQUEST_URI'])|| ($_SERVER['REQUEST_URI']=='/') ) { echo 'class="active"'; } ?> href="index.php">Home</a>
        <a  href="/db_options.php">Database</a>
        <a  href="/camera/index.html">Camera</a>
        <a  href="/filemanager/filemanager.php">File manager</a>
        <a  <?php  if(preg_match("/logs.php/",$_SERVER['REQUEST_URI']) ) { echo 'class="active"'; } ?> href="logs.php">Logs</a>        
      </div>
    </div>
