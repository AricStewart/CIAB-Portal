<meta http-equiv="Cache-control" content="No-Cache">
<script src="sitesupport/common.js"></script>
<script src="sitesupport/password.js"></script>
<script src="sitesupport/confirmbox.js"></script>
<script src="sitesupport/validation.js"></script>
<script type="text/javascript">
  function validateForm(thisform)
  {
    with (thisform)
    {
      if (validateRequired(user,"User must be filled out!")==false)
      {user.focus();return false}
      if (validateRequired(password,"Password must be entered!")==false)
      {password.focus();return false}
    }
  }
</script>
