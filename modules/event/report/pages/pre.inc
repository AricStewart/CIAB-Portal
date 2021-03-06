<?php

/*.
    require_module 'standard';
.*/

require_once($BACKEND."/RBAC.inc");

if (!\ciab\RBAC::havePermission("meeting.reports")) {
    goSite();
}

require_once __DIR__.'/../../functions/functions.inc';
require_once($FUNCTIONDIR."/reports.inc");

$reports = [];

$sql = 'SELECT * FROM `Events`';
$result = \DB::Run($sql);
$value = $result->fetch();
while ($value !== false) {
    $reports['Meeting Attendance Report '.$value['EventName']] = [
    'attendance_report',
    $value['EventID']
    ];
    $value = $result->fetch();
}

$sql = <<<SQL
    SELECT
        *
    FROM
        `AnnualCycles`
    WHERE
        (
        SELECT
            COUNT(`MeetingID`)
        FROM
            `OfficialMeetings`
        WHERE
            `Date` >= DateFrom AND `Date` <= DateTo
    );
SQL;

$result = \DB::Run($sql);
$value = $result->fetch();
while ($value !== false) {
    $reports['Cycle Attendance Report '.$value['AnnualCycleID']] = [
    'cycle_attendance_report',
    $value,
    ];
    $value = $result->fetch();
}

handle_report_request($reports);
