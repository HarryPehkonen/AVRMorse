;
; AVRMorse.asm
;
; Created: 2020-04-09 04:28:02
; Author : harry
;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.dseg

; use addresses as state
_morse_current_state:	.dw 0x0000

; use 0 and 1 bits as tokens
_morse_tokens:			.dw 0x0000

; for when transitioning from one state to another
_morse_handler:			.dw 0x0000

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.cseg

init:
	sbi DDRB, DDB4    ; pin 4 output

start:
	ldi r16, 'b'
	rcall morse_send
	rcall morse_character_separator
	ldi r16, 'a'
	rcall morse_send
done:
	rjmp done

morse_character_separator:
	rcall _morse_short_pause
	ret

morse_word_separator:
	rcall _morse_long_pause
	ret

; expect character to be in r16
morse_send:
	push r17
	push YL
	push YH
	push ZL
	push ZH

	ldi ZL, LOW(_morse_char_table<<1)
	ldi ZH, HIGH(_morse_char_table<<1)
_morse_find_letter:
	lpm r17, Z
	cp r16, r17 ; did we find the letter?
	breq _morse_found_letter
	adiw Z, 4 ; did not find letter.  skip to next letter
	rjmp _morse_find_letter
_morse_found_letter:
	adiw Z, 2 ; move to where morse code part for letter resides.
	lpm XL, Z+ ; get morse code for this letter
	lpm XH, Z+
	sts _morse_tokens, XL
	sts _morse_tokens + 1, XH

; morse state-machine
;
; need to keep track of:
;
;   o the code to send
;   o current state in the form of an address in cseg
;   o retain ability to indirectly jump to handler for the new state
;
; jumping to handler can be done with IJMP, which uses Z register.

	ldi YL, LOW(_morse_state_a<<1) ; start-state
	ldi YH, HIGH(_morse_state_a<<1)
	sts _morse_current_state, YL
	sts _morse_current_state + 1, YH

_morse_next:
	lds XL, _morse_tokens
	lds XH, _morse_tokens + 1
	lsl XL ; rotate msb of morse tokens into carry flag
	rol XH
	sts _morse_tokens, XL
	sts _morse_tokens + 1, XH

	lds YL, _morse_current_state
	lds YH, _morse_current_state + 1

	; from the state-table, do we use the entries for zero or one?
	; the token is still in the carry-flag from ROL.
	brbc SREG_C, _morse_zero

_morse_one:
	adiw Y, 4 ; move to where "1" token data is
_morse_zero:
	; no need to move since "0" token data is first

	; get handler address
	mov ZL, YL
	mov ZH, YH
	lsr ZH; >>1
	ror ZL
	sts _morse_handler, ZL
	sts _morse_handler + 1, ZH
	lsl ZL ; <<1 back
	rol ZH

	; what is the next state?
	adiw Z, 2 ; point at address of next state in state table
	lpm YL, Z+ ; load address of next state...
	lpm YH, Z+
	lsl YL
	rol YH

	sts _morse_current_state, YL; ...and save it for the next round
	sts _morse_current_state + 1, YH

	lds ZL, _morse_handler
	lds ZH, _morse_handler + 1
	ijmp ; execute the handler

_morse_return:
	pop ZH
	pop ZL
	pop YH
	pop YL
	pop r17
	ret
	
_morse_short_pause:
	push r16
	push r17

	clr r16
	clr r17

_morse_wait_longer:
	inc r16
	brne _morse_wait_longer
	inc r17
	brne _morse_wait_longer

	pop r17
	pop r16
	ret

; long pause is three times the length of a short pause
_morse_long_pause:
	push r16
	rcall _morse_short_pause
	rcall _morse_short_pause
	rcall _morse_short_pause
	pop r16
	ret

; state table
; 1:  handler for transition on token 0
; 2:  next state on token 0
; 3:  handler for transition on token 0
; 4:  next state on token 1
_morse_state_a:
	rjmp _morse_return
	.dw _morse_state_done
	rjmp _morse_next
	.dw _morse_state_b
_morse_state_b:
	rjmp _morse_dot
	.dw _morse_state_a
	rjmp _morse_next
	.dw _morse_state_c
_morse_state_c:
	rjmp _morse_dash
	.dw _morse_state_a
	rjmp _morse_error
	.dw _morse_state_error
_morse_state_error:
	rjmp _morse_error      ; dummy
	.dw _morse_state_error ; dummy
	rjmp _morse_error      ; dummy
	.dw _morse_state_error ; dummy
_morse_state_done:
	rjmp _morse_return    ; dummy
	.dw _morse_state_done ; dummy
	rjmp _morse_return    ; dummy
	.dw _morse_state_done ; dummy

_morse_char_table:
	_morse_char_a:	.dw 'a', 0b1011000000000000
	_morse_char_b:	.dw 'b', 0b1101010100000000
					.dw 'c', 0b1101011010000000

.equ _morse_char_entry_size = _morse_char_b - _morse_char_a

led_on:
	sbi PORTB, PORTB4
	ret

led_off:
	cbi PORTB, PORTB4
	ret

_morse_dot:
	rcall led_on
	rcall _morse_short_pause
	rcall led_off
	rcall _morse_short_pause
	rjmp _morse_next

_morse_dash:
	rcall led_on
	rcall _morse_long_pause
	rcall led_off
	rcall _morse_short_pause
	rjmp _morse_next

_morse_error:
	ret
