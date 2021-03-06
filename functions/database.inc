<?php

/*.
    require_module 'standard';
    require_module 'json';
.*/

// Currently, this database class is set for the MySQL PDO filter/backend. Future version will expand it to be agnostic

require_once(__DIR__.'/functions.inc');
require_once(__DIR__.'/locations.inc');
require_once($BACKEND.'/'.$_ENV['DB_BACKEND']); // MyPDO Definition used
require_once($BASEDIR.'/data/database-schema.php');

class DB
{

    private static $MODULE_TABLES = array();

    private static $MODULE_FOREIGNKEYS = array();

    private static $MODULE_PRIMARYKEYS = array();

    private static $MODULE_INDEX = array();

    /**
     * @var array
     */
    private $schema;

    /**
     * @var boolean
     */
    private $loaded = false;


    public static function addTables(array $tables): array
    {
        self::$MODULE_TABLES = array_merge(self::$MODULE_TABLES, $tables);
        return self::$MODULE_TABLES;

    }


    public static function addForeignKeys(array $fks): array
    {
        self::$MODULE_FOREIGNKEYS = array_merge(self::$MODULE_FOREIGNKEYS, $fks);
        return self::$MODULE_FOREIGNKEYS;

    }


    public static function addPrimaryKeys(array $pks): array
    {
        self::$MODULE_PRIMARYKEYS = array_merge(self::$MODULE_PRIMARYKEYS, $pks);
        return self::$MODULE_PRIMARYKEYS;

    }


    public static function addIndexes(array $indexes): array
    {
        self::$MODULE_INDEX = array_merge(self::$MODULE_INDEX, $indexes);
        return self::$MODULE_INDEX;

    }


    private static function verifyDB()
    {
        /* check if we have a database at all */
        $result = MyPDO::instance()->query(<<<sql
            SELECT SCHEMA_NAME
              FROM INFORMATION_SCHEMA.SCHEMATA
             WHERE SCHEMA_NAME = '{$_ENV['DBNAME']}';
sql
        );
        if ($result->fetch() === false) {
            return false;
        }

        $result = MyPDO::instance()->query("SHOW TABLES LIKE 'Configuration';");
        if ($result->fetch() === false) {
            return false;
        }

        return true;

    }


    private static function getCurrentDBVersion()
    {
        if (!DB::verifyDB()) {
            return false;
        }

        $sql = <<<SQL
            SELECT Value AS DBSchemaVersion
            FROM Configuration
            WHERE Field = 'DBSchemaVersion';
SQL;
        $result = self::run($sql);
        $value = $result->fetch();
        if ($value === false) {
            return false;
        } else {
            return $value['DBSchemaVersion'];
        }

    }


    private static function getRecordedDBSchemaMD5()
    {
        if (!DB::verifyDB()) {
            return false;
        }

        $sql = <<<SQL
            SELECT Value AS MD5
            FROM `Configuration`
            WHERE Field = 'DBSchemaMD5';
SQL;
        $result = self::run($sql);
        $value = $result->fetch();
        if ($value === false) {
            return false;
        } else {
            return $value['MD5'];
        }

    }


    private function buildMissingTables()
    {
        // This function cannot call the run function as we don't know if the activitylog table is created
        // Besides, this is maintenance, not user log tracking
        $DATADIR = $GLOBALS['BASEDIR'].'/data';

        // Capture a list of tables already created
        $result = MyPDO::instance()->query("SHOW TABLES");
        $arr = [];
        foreach ($result as $value) {
            foreach ($value as $row) {
                $arr[] = $row;
            }
        }
        // Verify every table we need exists, if not, create.
        foreach ($this->schema['tables'] as $table => $fields) {
            if (in_array($table, $arr)) {
                // Table exists
                // Capture a list of fields already in the table
                $build = "SHOW COLUMNS FROM $table";
                $result = MyPDO::instance()->query($build);
                $f_arr = [];
                foreach ($result as $value) {
                    $f_arr[] = $value['Field'];
                }

                // Check the list of fields to make sure we are set
                foreach ($fields as $column => $settings) {
                    if (! in_array($column, $f_arr)) {
                        // Missing field, alter-add it
                        $build = "ALTER TABLE ".$table;
                        $build .= " ADD ".$column." ".$settings;
                        MyPDO::instance()->query($build);
                    }
                }
            } else {
                // Missing a table, build it!
                $build = 'CREATE TABLE '.$table.' (';
                foreach ($fields as $column => $setting) {
                    $build .= $column.' '.$setting.', ';
                }
                $build = rtrim($build, ', ').");";
                MyPDO::instance()->query($build);

                // If there is seed available for this table, seed it!
                if (file_exists($DATADIR.'/DBSeed/'.$table.'.sql')) {
                    $sql_data = file_get_contents($DATADIR.'/DBSeed/'.$table.'.sql');
                    MyPDO::instance()->query($sql_data);
                }
                $modules = glob($GLOBALS['MODULESDIR'].'/*/DBSeed/'.$table.'.sql');
                if (is_array($modules) && !empty($modules)) {
                    foreach ($modules as $module) {
                        $sql_data = @file_get_contents($module);
                        MyPDO::instance()->query($sql_data);
                    }
                }
            }
        }

        //Verify primary keys
        foreach ($this->schema['primaryKeys'] as $table => $keys) {
            $query = "SHOW KEYS FROM `$table` WHERE Key_name = 'PRIMARY';";
            $result = MyPDO::instance()->query($query);
            $value = $result->fetch();
            if ($value === false) {
                $build = "ALTER TABLE `".$table."` ADD PRIMARY KEY (".implode($keys, ', ').");";
                $result = MyPDO::instance()->query($build);
            }
        }

        // Capture a list of all Constraints (Foreign Keys)
        $build = "SELECT TABLE_NAME, COLUMN_NAME FROM information_schema.KEY_COLUMN_USAGE";
        $build .= " WHERE TABLE_SCHEMA = '".$_ENV['DBNAME']."' AND CONSTRAINT_NAME <> 'PRIMARY';";
        $result = MyPDO::instance()->query($build);
        $arr = [];
        foreach ($result as $value) {
            $arr[] = $value['TABLE_NAME'].":".$value['COLUMN_NAME'];
        }
        // Verify every foreign key we need exists, if not, create.
        foreach ($this->schema['foreignKeys'] as $table => $fields) {
            foreach ($fields as $column => $referto) {
                $lookfor = $table.":".$column;
                if (!in_array($lookfor, $arr)) {
                    $build = "ALTER TABLE ".$table;
                    $build .= " ADD FOREIGN KEY (".$column.") REFERENCES ".$referto.";";
                    MyPDO::instance()->query($build);
                }
            }
        }

        // Manage indexes
        foreach ($this->schema['index'] as $table => $indexes) {
            $build = "SHOW INDEXES FROM `".$table."`;";
            $result = mypdo::instance()->query($build);
            $current_indexes = [];
            foreach ($result as $value) {
                $current_indexes[] = $value['Key_name'];
            }

            $desired_indexes = array_keys($indexes);
            $indexes_to_add = array_diff($desired_indexes, $current_indexes);

            foreach ($indexes_to_add as $index_to_add) {
                $columns = $indexes[$index_to_add];
                $sql = "CREATE INDEX ".$index_to_add." ON ".$table." (`".implode("`, `", $columns)."`);";
                $result = MyPDO::instance()->query($sql);
                if ($result->errorCode() != '00000') {
                    error_log("Index update failed for ${table} ${index_to_add}: $result->errorCode()");
                }
            }
        }

    }


    public function loadModules()
    {
        if ($this->loaded) {
            return;
        }
        if (array_key_exists('MODULESDIR', $GLOBALS) &&
            !empty($GLOBALS['MODULESDIR'])) {
            try {
                $modules = scandir($GLOBALS['MODULESDIR']);
                foreach ($modules as $key => $value) {
                    if (!in_array($value, array(".", ".."))) {
                        if (is_dir($GLOBALS['MODULESDIR'].DIRECTORY_SEPARATOR.$value)) {
                            if (is_file($GLOBALS['MODULESDIR'].DIRECTORY_SEPARATOR.$value.DIRECTORY_SEPARATOR.'database-schema.inc')) {
                                require_once($GLOBALS['MODULESDIR'].DIRECTORY_SEPARATOR.$value.DIRECTORY_SEPARATOR.'database-schema.inc');
                            }
                        }
                    }
                }
            } catch (RuntimeException $e) {
                error_log("ERROR!!! Database Schema import failed: ".$e);
            }
        }

        $this->schema['tables'] = array_merge($this->schema['tables'], self::$MODULE_TABLES);
        $this->schema['foreignKeys'] = array_merge($this->schema['foreignKeys'], self::$MODULE_FOREIGNKEYS);
        $this->schema['primaryKeys'] = array_merge($this->schema['primaryKeys'], self::$MODULE_PRIMARYKEYS);
        $this->schema['index'] = array_merge($this->schema['index'], self::$MODULE_INDEX);
        $this->loaded = true;

    }


    public function __construct()
    {
        $this->schema = array();
        $this->schema['tables'] = SCHEMA::$DB_tables;
        $this->schema['foreignKeys'] = SCHEMA::$DB_foreignKeys;
        $this->schema['primaryKeys'] = SCHEMA::$DB_primaryKeys;
        $this->schema['index'] = SCHEMA::$DB_index;
        $this->loadModules();
        $oldMD5 = DB::getRecordedDBSchemaMD5();
        $currentMD5 = md5(json_encode($this->schema));

        if ($oldMD5 != $currentMD5) {
            $this->buildMissingTables();
            $sql = <<<SQL
                REPLACE INTO Configuration
                    (`Value`, `Field`)
                VALUES
                    ('$currentMD5', 'DBSchemaMD5');
SQL;
            DB::run($sql);
        }

        if (!empty($_REQUEST['Function']) && $_REQUEST['Function'] != 'update') {
            $upgrade = DB::getCurrentDBVersion();
            if ($upgrade != SCHEMA::$REQUIED_DB_SCHEMA) {
            // If we have to do an update, do it
                header("Location: ".$BASEURL."/index.php?Function=update&from=".$upgrade."&to=".SCHEMA::$REQUIED_DB_SCHEMA);
            }
        }

    }


    private static function dbLogEntry($sql)
    {
        if (substr(ltrim($sql), 0, 6) !== "SELECT") {
            // Prepare a log entry for this query
            $logMessage = MyPDO::instance()->prepare('INSERT INTO ActivityLog (AccountID, Function, Query) VALUES (:account, :function, :query);');

            if (empty($_SESSION['accountId'])) {
                $account = 0;
            } else {
                $account = $_SESSION['accountId'];
            }
            if (empty($_SESSION['Function'])) {
                $function = "public";
            } else {
                $function = $_SESSION['Function'];
            }

            // Use PDO to make the SQL Safe as it IS a SQL statement we are trying to capture
            $logMessage->bindParam(':account', $account, PDO::PARAM_INT);
            $logMessage->bindParam(':function', $function, PDO::PARAM_STR);
            $logMessage->bindParam(':query', $sql, PDO::PARAM_STR);

            // Write the log entry
            $logMessage->execute();
        }

    }


    public static function run($sql, $args = [])
    {
        if (!(array_key_exists('SILENCE_LOG', $GLOBALS) && $GLOBALS['SILENCE_LOG'])) {
            DB::dbLogEntry($sql); // Capture all queries in the ActivityLog
        }
        if (!$args) {
            return MyPDO::instance()->query($sql);
        }
        $stmt = MyPDO::instance()->prepare($sql);
        $stmt->execute($args);
        return $stmt;

    }


   /* end */
}


function lookup_cycleID($date)
{
    if ($date instanceof DateTime) {
        $year  = date("Y-m-d", $date);
    } else {
        $time = strtotime($date);
        $year  = date("Y-m-d", $time);
    }
    $sql = "SELECT AnnualCycleID FROM AnnualCycles WHERE DateTo >= '$year' AND DateFrom <= '$year';";
    $result = DB::run($sql);
    $value = $result->fetch();
    if ($value) {
        $cycle = $value['AnnualCycleID'];
        return $cycle;
    }
    return null;

}


$_currentCycle = null;
$_currentEvent = null;
$_currentEventName = null;


function current_cycleID()
{
    global $_currentCycle, $_SESSION;

    if ($_currentCycle == null) {
        /* Place 1 - Look for it in the session */

        if (isset($_SESSION['CurrentCycle'])) {
            return $_SESSION['CurrentCycle'];
        }

        /* Place 2 - try to get it from the database */

        $sql = "SELECT Value FROM Configuration WHERE Field='CurrentCycleID';";
        $result = DB::run($sql);
        $value = $result->fetch();
        if ($value) {
            $_currentCycle = $value['CurrentCycleID'];
            return $_currentCycle;
        }

        /* Place 3 - current calander year */

        $year = date("Y-m-d");
        $sql = "SELECT AnnualCycleID FROM AnnualCycles WHERE DateTo > '$year';";
        $result = DB::run($sql);
        $value = $result->fetch();
        if ($value) {
            $_currentCycle = $value['AnnualCycleID'];
            return $_currentCycle;
        }

        /* Fallback - Last year in database*/

        $sql = "SELECT AnnualCycleID FROM AnnualCycles ORDER BY AnnualCycleID DESC LIMIT 1;";
        $result = DB::run($sql);
        $value = $result->fetch();
        if ($value) {
            $_currentCycle = $value['AnnualCycleID'];
            return $_currentCycle;
        }

        return null;
    }
    return $_currentCycle;

}


function current_eventID()
{
    global $_currentEvent, $_SESSION;

    if ($_currentEvent == null) {
        /* Place 1 - Look for it in the session */

        if (isset($_SESSION['CurrentEvent'])) {
            return $_SESSION['CurrentEvent'];
        }

        /* Place 2 - try to get it from the database */

        $sql = "SELECT Value FROM Configuration WHERE Field='CurrentEventID';";
        $result = DB::run($sql);
        $value = $result->fetch();
        if ($value) {
            $_currentEvent = $value['CurrentEventID'];
            return $_currentEvent;
        }

        /* Place 3 - current calander year */

        $year = date("Y-m-d");
        $sql = "SELECT EventID FROM Events WHERE DateTo >= '$year';";
        $result = DB::run($sql);
        $value = $result->fetch();
        if ($value) {
            $_currentEvent = $value['EventID'];
            return $_currentEvent;
        }

        /* Fallback - Last year in database*/

        $sql = "SELECT EventID FROM Events ORDER BY EventID DESC LIMIT 1;";
        $result = DB::run($sql);
        $value = $result->fetch();
        if ($value) {
            $_currentEvent = $value['EventID'];
            return $_currentEvent;
        }

        return null;
    }
    return $_currentEvent;

}


function current_eventName()
{
    global $_currentEventName, $CONHOST;

    if ($_currentEventName === null) {
        $event = current_eventID();
        $sql = "SELECT `EventName` FROM Events WHERE EventID=$event;";
        try {
            $result = DB::run($sql);
            $value = $result->fetch();
            if ($value) {
                $_currentEventName = $value['EventName'];
                return $_currentEventName;
            } else {
                return $CONHOST;
            }
        } catch (PDOException $e) {
            return $CONHOST;
        }
    }
    return $_currentEventName;

}


function lookup_names(&$data, $column_name)
{
    /* HACK Lookup AccountIDs */
    if (in_array($column_name, $data[0])) {
        $data[0][] = $column_name.'Name';
        $colmn = array_search($column_name, $data[0]);
        $ids = array_column($data, $colmn);
        array_shift($ids);
        $users = lookup_users_by_ids(array_unique($ids));
        $usr = [];
        foreach ($users['users'] as $user) {
            $usr[$user['Id']] = $user['First Name'].' '.$user['Last Name'];
        }
        $idx = 1;
        foreach ($data as $id => $row) {
            $data[$idx][] = $usr[$data[$idx][$colmn]];
            $idx += 1;
        }
    }

}


function dump_sql_to_csv($sql, $lookup_account = false, $filter = null)
{
    $data = [];
    $data[] = array();
    $result = DB::run($sql);
    for ($i = 0; $i < $result->columnCount(); $i++) {
        $data[0][] = $result->getColumnMeta($i)['name'];
    }
    $value = $result->fetch();
    while ($value !== false) {
        $data[] = array_values($value);
        $value = $result->fetch();
    }

    if ($lookup_account) {
        lookup_names($data, 'AccountID');
        lookup_names($data, 'AuthorizedByID');
        lookup_names($data, 'EnteredByID');
    }

    $fh = fopen('php://output', 'w');
    ob_start();

    foreach ($data as $line) {
        if ($filter !== null) {
            $replace = '\1<REDACTED>\3';
            fputcsv($fh, preg_replace($filter, $replace, $line));
        } else {
            fputcsv($fh, $line);
        }
    }
    $text = ob_get_clean();

    return $text;

}


function dump_table_to_csv($table, $condition = null, $filter = null)
{
    $sql = "SELECT * FROM `$table` $condition;";
    return dump_sql_to_csv($sql, false, $filter);

}


function load_table($sql, $association = null)
{
    $result = DB::run($sql);
    $value = $result->fetch();
    $data = array();
    while ($value !== false) {
        if ($association !== null) {
            $entry = [];
            foreach ($association as $key => $dbkey) {
                $entry[$key] = $value[$dbkey];
            }
            $data[] = $entry;
        } else {
            $data[] = $value;
        }
        $value = $result->fetch();
    }
    return $data;

}
