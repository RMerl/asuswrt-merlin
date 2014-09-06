$j(document).ready(function(){
	$j('.wfps_form_action').each(function() {
		$j(this).click(function(e) {
			e.preventDefault();
			$targetObj = $j('#'+ $j(this).attr('data-change'));
			if ($targetObj.attr('type') == 'password') {
				$targetObj.attr('type','text');
				$j(this).text('Hide');
			}
			else {
				$targetObj.attr('type','password');
				$j(this).text('Show');
			}
		});
	});
	
	
	$j('input[data-limit-low], input[data-limit-high]').each(function () {
		$j(this).keyup(function() {
			curVal = $j(this).val();
			curId = $j(this).attr('id');
			limLo = $j(this).attr('data-limit-low');
			limHi = $j(this).attr('data-limit-high');
			if ((curVal.length > limHi || curVal.length < limLo) && curVal.length != 0) {		//length of password less than 8
				$j(this).attr('data-error','true');
				if (!$j('#error-'+ curId).length) {
					$j(this).after('<div class="error_msg" id="error-'+ curId +'"></div>').css('display','block');
					$j(this).addClass('error');
				}
				$j('#error-'+ curId).text('Please enter between '+ limLo +' and '+ limHi +' characters.').css('display','block');
			}
			else{
				if ($j('#error-'+ curId)) {
					$j('#error-'+ curId).text('').css('display','none');
					$j(this).removeClass('error');
				}
				
				if($j('#wfps_password1')[0].value != $j('#wfps_password2')[0].value){		//To check password match
					$j('#wfps_password1').attr('data-error','true');
					$j('#wfps_password2').attr('data-error','true');
					if (!$j('#error-wfps_password1').length) {
						$j('#wfps_password1').after('<div class="error_msg" id="error-wfps_password1"></div>').css('display','block');
						$j('#wfps_password1').addClass('error');
					}
					
					if (!$j('#error-wfps_password2').length) {
						$j('#wfps_password2').after('<div class="error_msg" id="error-wfps_password2"></div>').css('display','block');
						$j('#wfps_password2').addClass('error');
					}
					$j('#error-wfps_password1').text('Your new password entries must match').css('display','block');
					$j('#error-wfps_password2').text('Your new password entries must match').css('display','block');
				}
				else{
					if ($j('#error-wfps_password1')) {
						$j('#error-wfps_password1').text('').css('display','none');
						$j('#wfps_password1').removeClass('error');
					}
					
					if ($j('#error-wfps_password2')) {
						$j('#error-wfps_password2').text('').css('display','none');
						$j('#wfps_password2').removeClass('error');
					}
				}
			}
		});
	});
	
	$j('form[data-match]').each(function() {
		$j(this).submit(function () {
			if ($j('input[name="password1"]').val() == $('input[name="password2"]').val()) {
				return true;
			}
			else {
				if (!$j('#error-match').length) {
					$j('input[name="password2"]').after('<div class="error_msg" id="error-match"></div>').css('display','block');
				}
				$j('#error-match').text('Passwords don\'t match!');
				$j('input[name="password1"], input[name="password2"]').addClass('error');
				return false;
			}
		});
	});
	
	$j('input[type="password"]').each(function() {
		$j(this).focus(function() {
			$j('.wfps_form_action[data-change="'+$j(this).attr('id')+'"]').css('display','inline-block');
		});	
		$j(this).blur(function() {
			if ($j(this).val().length <= 0) {
				$j('.wfps_form_action[data-change="'+$j(this).attr('id')+'"]').css('display','none');
			}
		});
	});
	
});