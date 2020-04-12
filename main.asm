init:
	sbi DDRB, DDB4    ; pin 4 output

start:

	; initialize ram where things like callback addresses are
	; kept.  if you don't do this, avrmorse won't know how to
	; send dots and dashes, and will absolutely crash.
	rcall morse_init

	; tell avrmorse how to *start* sending a dot or dash.
	; if you don't do this, avrmorse will just return safely.
	ldi ZL, LOW(led_on)
	ldi ZH, HIGH(led_on)
	rcall morse_set_on_callback

	; tell avrmorse how to *stop* sending a dot or dash.
	; if you don't do this, avrmorse will just return safely.
	ldi ZL, LOW(led_off)
	ldi ZH, HIGH(led_off)
	rcall morse_set_off_callback

	; put your ascii code into r16 and send away.  if avrmorse
	; doesn't know how to send the character, it will just
	; return safely.  maybe you will notice the missing character.
	ldi r16, 'b'
	rcall morse_send
	rcall morse_send_character_separator
	ldi r16, 'a'
	rcall morse_send
done:
	rjmp done

led_on:
	sbi PORTB, PORTB4
	ret

led_off:
	cbi PORTB, PORTB4
	ret

some_text:
	.db "harri", 0

.include "./AVRMorse.inc"

