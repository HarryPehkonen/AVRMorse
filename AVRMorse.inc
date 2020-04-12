;
; AVRMorse.inc
;
; Created: 2020-04-09 04:28:02
; Author : harri a. pehkonen
;

; use registers safely (push and pop) and without .def so that
; whatever main program can use .def on any register.

; "private" addresses should have leading underscore.

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.dseg

; use address inside state table as state.
_morse_current_state:	.dw 0x0000

; use 0 and 1 bits as characters.  keep in an "easily readable format"
; by writing from left to right (i.e. first work with the 1 in
; b1000000000000000)
;
; tokens:
;   1 -- dot
;   11 -- dash
;   0 -- separates dots and dashes
;   00 -- the end

_morse_tokens:			.dw 0x0000

; for when transitioning from one state to another
_morse_handler:			.dw 0x0000

; user must supply the address of a routine that will turn on
; whatever signalling device is used (attached to a pin).
_morse_on_callback:		.dw 0x0000

; routine to turn off signalling device
_morse_off_callback:	.dw 0x0000

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.cseg

morse_send_character_separator:
	rcall _morse_short_pause
	ret

morse_send_word_separator:
	rcall _morse_long_pause
	ret

; until the user supplies one, use a dummy callback for turning
; signalling device on and off
_morse_dummy_callback:
	ret

morse_init:
	push ZL
	push ZH

	ldi ZL, LOW(_morse_dummy_callback)
	ldi ZH, HIGH(_morse_dummy_callback)
	rcall morse_set_on_callback
	rcall morse_set_off_callback

	pop ZH
	pop ZL
	ret

; expect callback in Z register
morse_set_on_callback:
	sts _morse_on_callback, ZL
	sts _morse_on_callback + 1, ZH
	ret

; expect callback in Z register
morse_set_off_callback:
	sts _morse_off_callback, ZL
	sts _morse_off_callback + 1, ZH
	ret

; expect character to be in r16
morse_send:
	push r17
	push YL
	push YH
	push ZL
	push ZH

	; load the address of char table into Z
	ldi ZL, LOW(_morse_char_table<<1)
	ldi ZH, HIGH(_morse_char_table<<1)
_morse_find_letter:
	lpm r17, Z

	; if we don't have this character in char table,
	; silently ignore it by returning.  we will know
	; we have reached the end of the char table if
	; we found a zero in there.
	tst r17 ; have we reached the end?
	brne _morse_not_at_end_of_char_table
	rjmp _morse_return ; return cleanly (pop)

_morse_not_at_end_of_char_table:
	cp r16, r17 ; did we find the letter?
	breq _morse_found_letter
	adiw Z, 4 ; did not find letter yet.  skip to next letter
	rjmp _morse_find_letter

_morse_found_letter:
	adiw Z, 2 ; move to where morse code part for letter resides.
	lpm r17, Z+ ; get morse code for this letter
	sts _morse_tokens, r17
	lpm r17, Z+
	sts _morse_tokens + 1, r17

; morse state-machine
;
; need to keep track of:
;
;   o the rest of morse code to send
;   o current state in the form of an address in state table in cseg
;   o retain ability to indirectly jump to handler for the new state
;
; jumping to handler can be done with IJMP, which uses Z register.

	; start-state is the first state in the table
	ldi YL, LOW(_morse_state_a<<1)
	ldi YH, HIGH(_morse_state_a<<1)
	sts _morse_current_state, YL ; save state for later
	sts _morse_current_state + 1, YH

_morse_next:
	; get the next bit to process.  save the rest for later
	lds ZL, _morse_tokens
	lds ZH, _morse_tokens + 1
	lsl ZL ; rotate msb of morse tokens into carry flag
	rol ZH ; note:  16 bits are rotated
	sts _morse_tokens, ZL
	sts _morse_tokens + 1, ZH

	; the state table has two entries for each state.  one for
	; when encountering a zero, and another for when encountering
	; a one.  not sure which one we want yet, so start at beginning
	lds YL, _morse_current_state
	lds YH, _morse_current_state + 1

	; from the state-table, do we use the entries for zero or one?
	; the token is still in the carry-flag from ROL.
	brbc SREG_C, _morse_zero

_morse_one:
	adiw Y, 4 ; move to where "1" token data is
_morse_zero:
	; no need to move since "0" token data is first

	; get handler address.  the compiler gave us the byte address,
	; but we need a word address.  convert to word address by
	; shifting to the right.  IJMP uses word addresses, but
	; many other instructions want byte addresses.
	mov ZL, YL
	mov ZH, YH
	lsr ZH; to word address by >>1
	ror ZL
	sts _morse_handler, ZL
	sts _morse_handler + 1, ZH
	lsl ZL ; <<1 back to byte address
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

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
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

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; long pause is three times the length of a short pause
_morse_long_pause:
	rcall _morse_short_pause
	rcall _morse_short_pause
	rcall _morse_short_pause
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

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
_morse_char_table:
	_morse_char_a:	.dw 'a', 0b1011000000000000
	_morse_char_b:	.dw 'b', 0b1101010100000000
					.dw 'c', 0b1101011010000000
					.dw 'd', 0b1101010000000000
					.dw 'e', 0b1000000000000000
					.dw 'f', 0b1010110100000000
					.dw 'g', 0b1101101000000000
					.dw 'h', 0b1010101000000000
					.dw 'i', 0b1010000000000000
					.dw 'j', 0b1011011011000000
					.dw 'k', 0b1101011000000000
					.dw 'l', 0b1011010100000000
					.dw 'm', 0b1101100000000000
					.dw 'n', 0b1101000000000000
					.dw 'o', 0b1101101100000000
					.dw 'p', 0b1011011010000000
					.dw 'q', 0b1101101011000000
					.dw 'r', 0b1011010000000000
					.dw 's', 0b1010100000000000
					.dw 't', 0b1100000000000000
					.dw 'u', 0b1010110000000000
					.dw 'v', 0b1010101100000000
					.dw 'w', 0b1011011000000000
					.dw 'x', 0b1101010110000000
					.dw 'y', 0b1101011011000000
					.dw 'z', 0b1101101010000000
					.dw '1', 0b1011011011011000
					.dw '2', 0b1010110110110000
					.dw '3', 0b1010101101100000
					.dw '4', 0b1010101011000000
					.dw '5', 0b1010101010000000
					.dw '6', 0b1101010101000000
					.dw '7', 0b1101101010100000
					.dw '8', 0b1101101101010000
					.dw '9', 0b1101101101101000
					.dw '0', 0b1101101101101100
					.dw 0x0000, 0x0000 ; end of character table

.equ _morse_char_entry_size = _morse_char_b - _morse_char_a

_morse_dot:
	;rcall led_on
	lds ZL, _morse_on_callback
	lds ZH, _morse_on_callback + 1
	icall

	rcall _morse_short_pause

	;rcall led_off
	lds ZL, _morse_off_callback
	lds ZH, _morse_off_callback + 1
	icall

	rcall _morse_short_pause
	rjmp _morse_next

_morse_dash:
	;rcall led_on
	lds ZL, _morse_on_callback
	lds ZH, _morse_on_callback + 1
	icall

	rcall _morse_long_pause

	;rcall led_off
	lds ZL, _morse_off_callback
	lds ZH, _morse_off_callback + 1
	icall

	rcall _morse_short_pause
	rjmp _morse_next

_morse_error:
	ret
