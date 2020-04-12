init:
	sbi DDRB, DDB4    ; pin 4 output

start:
	rcall morse_init

	ldi ZL, LOW(led_on)
	ldi ZH, HIGH(led_on)
	rcall morse_set_on_callback

	ldi ZL, LOW(led_off)
	ldi ZH, HIGH(led_off)
	rcall morse_set_off_callback

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

