/*
 * Javascript for volhours
 */

/* jshint browser: true */

'use strict';

var volPast = null;

function calculateHours() {
  var hours = Number(document.getElementById('hours').value);
  var am = Number(document.getElementById('minutes').value);
  hours += am;
  var creditHours = hours * document.getElementById('modifier').value;
  var tag = ' hours';
  if (hours == 1) {
    tag = ' hour';
  }
  document.getElementById('totalhours').innerHTML = creditHours + tag;
  document.getElementById('actualhours').value = Number(hours);
  checkHours();
}

function resetForm() {
  userLookup.clear();
  document.getElementById('submitbtn').disabled = true;
  document.getElementById('volunteername').innerHTML = 'a Volunteer';
  document.getElementById('lookupname').innerHTML = '';

  /* Clean out old highlighted departments */
  var depts = document.getElementById('department');
  for (var i = 0; i < depts.length; i++) {
    if (depts[i].text[0] == '*' || depts[i].text[0] == '-') {
      depts[i + 1].selected = true;
      depts.remove(i);
      i--;
    }
  }
  document.getElementById('message').innerHTML = '';
  document.getElementById('datetime').classList.remove('w3-red');
}

function formatTime(time) {

  var days = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday',
              'Saturday'];
  var pmam = null;
  if (time.getHours() >= 12) {
    pmam = ' PM';
  } else {
    pmam = ' AM';
  }
  var hours = time.getHours() % 12;
  if (hours === 0) {
    hours = 12;
  }
  var min = time.getMinutes();
  if (min < 10) {
    min = '0' + min;
  } else {
    min = min.toString();
  }

  return (days[time.getDay()] + ' ' + hours.toString() + ':' + min + pmam);
}

function checkHours() {
  document.getElementById('datetime').classList.remove('w3-red');
  document.getElementById('message').innerHTML = '';

  if (volPast && volPast.length) {
    var hours = Number(document.getElementById('hours').value);
    var mins  = Number(document.getElementById('minutes').value);
    var newEnd = new Date(document.getElementById('datetime').value);
    var newBegin = new Date(document.getElementById('datetime').value);
    newBegin.setHours(newBegin.getHours() - hours);
    newBegin.setMinutes(newBegin.getMinutes() - mins);

    for (var i = 0; i < volPast.length; i++) {
      var _shift = volPast[i];
      var _end = new Date(_shift['End Date Time']);
      var _begin = new Date(_shift['End Date Time']);
      _begin.setHours(_begin.getHours() - Math.floor(_shift['Actual Hours']));
      _begin.setMinutes(_begin.getMinutes() -
        Math.floor((_shift['Actual Hours'] % 1) * 60));

      if ((newBegin < _end && newBegin > _begin) ||
          (newEnd < _end && newEnd > _begin) ||
          (newEnd >= _end && newBegin <= _begin)) {
        document.getElementById('datetime').classList.add('w3-red');
        document.getElementById('message').innerHTML = 'Overlapping with ' +
            _shift['Department Worked'] + ' ( ' + formatTime(_begin) +
            ' - ' + formatTime(_end) + ' ) ';
        break;
      }
    }
  }
}

function onSuccess(target, resp) {
  document.getElementById('datetime').classList.remove('w3-red');
  document.getElementById('message').innerHTML = '';

  var response = resp[0];
  var name = response['First Name'] + ' ' + response['Last Name'];
  var uid = response.Id;
  if (response.ConCom) {
    userLookup.markFailure();
    document.getElementById('submitbtn').disabled = true;
    document.getElementById('volunteername').innerHTML = 'a Volunteer';
    document.getElementById('lookupname').innerHTML = name +
        ' is on ConCom (' + uid + ')';
  } else {
    userLookup.clearFailure();
    userLookup.set(uid);
    document.getElementById('submitbtn').disabled = false;
    document.getElementById('volunteername').innerHTML = name + ' (' +
        uid + ')';
    document.getElementById('lookupname').innerHTML = name + ' (' + uid + ')';

    var depts = document.getElementById('department');

    /* Clean out old highlighted departments */
    for (i = 0; i < depts.length; i++) {
      if (depts[i].text[0] == '*' || depts[i].text[0] == '-') {
        depts[i + 1].selected = true;
        depts.remove(i);
        i--;
      }
    }
    if ('volunteer' in response) {
      var past = [];
      var option = document.createElement('option');
      option.text = '------------------';
      option.disabled = true;
      depts.add(option, 0);

      for (var i = 0, len = response.volunteer.length; i < len; i++) {
        var dept = response.volunteer[i];
        if (past.indexOf(dept['Department Worked']) == -1) {
          option = document.createElement('option');
          option.selected = true;
          option.value = dept['Department Worked'];
          option.text = '* ' + dept['Department Worked'];
          depts.add(option, 0);
          past.push(dept['Department Worked']);
        }
      }
    }
    if ('volunteer' in response) {
      volPast = response.volunteer;
      checkHours();
    }
  }
}