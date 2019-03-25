; naskfunc
; TAB=4

[FORMAT "WCOFF"]				; �I�u�W�F�N�g�t�@�C������郂�[�h	
[INSTRSET "i486p"]				; 486�̖��߂܂Ŏg�������Ƃ����L�q
[BITS 32]						; 32�r�b�g���[�h�p�̋@�B�����点��
[FILE "naskfunc.nas"]			; �\�[�X�t�@�C�������

		GLOBAL	_io_hlt, _io_cli, _io_sti, _io_stihlt
		GLOBAL	_io_in8, _io_in16, _io_in32
		GLOBAL	_io_out8, _io_out16, _io_out32
		GLOBAL	_io_load_eflags, _io_store_eflags
		GLOBAL	_load_gdtr, _load_idtr
		GLOBAL	_asm_inthandler21, _asm_inthandler27,  
		GLOBAL	_asm_inthandler20, _asm_inthandler2c, 
		GLOBAL	_asm_inthandler0d, _asm_inthandler0c
		GLOBAL	_load_cr0, _store_cr0
		GLOBAL	_memtest_sub
		GLOBAL	_load_tr
		GLOBAL	_farjmp, _farcall
		GLOBAL	_asm_cons_putchar
		GLOBAL	_asm_hrb_api
		GLOBAL	_start_app, _asm_end_app
		EXTERN	_inthandler21, _inthandler27, _inthandler20
		EXTERN	 _inthandler2c, _inthandler0d
		EXTERN	_cons_putchar
		EXTERN 	_hrb_api

[SECTION .text]

_io_hlt:	; void io_hlt(void);
		HLT
		RET

_io_cli:	; void io_cli(void);
		CLI
		RET

_io_sti:	; void io_sti(void);
		STI
		RET

_io_stihlt:	; void io_stihlt(void);
		STI
		HLT
		RET

_io_in8:	; int io_in8(int port);
		MOV		EDX,[ESP+4]		;port
		MOV		EAX,0
		IN		AL,DX
		RET

_io_in16:	; int io_in16(int port);
		MOV		EDX,[ESP+4]		;port
		MOV		EAX,0
		IN		AX,DX
		RET

_io_in32:	; int io_in32(int port);
		MOV		EDX,[ESP+4]			;port
		IN		EAX,DX
		RET

_io_out8:	; void io_out8(int port, int data);
		MOV		EDX,[ESP+4]			;port
		MOV		AL,[ESP+8]			;data
		OUT		DX,AL
		RET

_io_out16:	; void io_out16(int port, int data);
		MOV		EDX,[ESP+4]			;port
		MOV		EAX,[ESP+8]			;data
		OUT		DX,AX
		RET

_io_out32:	; void io_out32(int port, int data);
		MOV		EDX,[ESP+4]			;port
		MOV		EAX,[ESP+8]			;data
		OUT		DX,EAX
		RET

_io_load_eflags:	; int io_load_eflags(void);
		PUSHFD		; PUSH EFLAGS ということ
		POP		EAX
		RET

_io_store_eflags:	; void io_store_eflags(int eflags);
		MOV		EAX,[ESP+4]
		PUSH		EAX
		POPFD		;pop eflags
		RET
		
_load_gdtr:		; void load_gdtr(int limit, int addr);
		MOV		AX,[ESP+4]		;limit
		MOV		[ESP+6],AX
		LGDT	[ESP+6]
		RET

_load_idtr:		; void load_idtr(int limit, int addr);
		MOV		AX,[ESP+4]		;limit
		MOV		[ESP+6],AX
		LIDT	[ESP+6]
		RET

_asm_inthandler21:		
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX, ESP
		PUSH	EAX		; 割り込まれたときのEAXを保持
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler21
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		IRETD

_asm_inthandler27:		
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX, ESP
		PUSH	EAX		; 割り込まれたときのEAXを保持
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler27
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		IRETD

_asm_inthandler2c:		
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX, ESP
		PUSH	EAX		; 割り込まれたときのEAXを保持
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler2c
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		IRETD

_load_cr0:			; int load_cr0(void);
		MOV		EAX, CR0
		RET

_store_cr0:			; void store_cr0(int cr0);
		MOV		EAX,[ESP+4]
		MOV		CR0,EAX
		RET

_memtest_sub:		; unsigned int memtest_sub(unsigned int start, unsigned int end);
		PUSH	EDI
		PUSH	ESI
		PUSH	EBX
		MOV		ESI, 0xaa55aa55			; pat0 = 0xaa55aa55
		MOV		EDI, 0x55aa55aa			; pat1 = 0x55aa55aa
		MOV		EAX, [ESP+12+4]			; i = start;
mts_loop:
		MOV		EBX,EAX	
		ADD		EBX,0xffc				; p = i + 0xffc;
		MOV		EDX,[EBX]				; old = *p;
		MOV		[EBX],ESI				; *p = pat0;
		XOR		DWORD [EBX], 0xffffffff	; *p ^= 0xffffffff;
		CMP		EDI, [EBX]				; if(*p != pat1) goto fin;
		JNE		mts_fin					
		XOR		DWORD [EBX], 0xffffffff	; *p ^= 0xffffffff;
		CMP		ESI, [EBX]				; if(*p != pat0) goto fin;
		JNE		mts_fin					
		MOV		[EBX],EDX				; *p = old;
		ADD		EAX,0x1000				; i += 0x1000;
		CMP		EAX, [ESP+12+8]			; if(i<=end) goto mts_loop;
		JBE		mts_loop
		POP		EBX
		POP		ESI
		POP		EDI
		RET
mts_fin:
		MOV		[EBX],EDX
		POP		EBX
		POP		ESI
		POP		EDI
		RET

_asm_inthandler20:		
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX, ESP
		PUSH	EAX		; 割り込まれたときのEAXを保持
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler20
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		IRETD

_load_tr:		; void load_tr(int tr);
		LTR		[ESP+4] ;tr
		RET

_farjmp:		; void farjmp(int eip, int cs);
		JMP		FAR [ESP+4]	;eip, cs
		RET

_farcall:		; void farcall(int eip, int cs);
		CALL	FAR [ESP+4]
		RET
		
_asm_cons_putchar:
		STI
		PUSHAD
		PUSH	1
		AND		EAX,0xff ;AHやEAXの上位を0にして、EAXに文字コードが入った状態にする
		PUSH	EAX
		PUSH	DWORD [0x0fec]	;(consの番地)
		CALL	_cons_putchar
		ADD		ESP,12	;スタックに入ったデータを捨てる
		POPAD
		IRETD

_start_app:		; void start_app(int eip, int cs, int esp, int ds, int *tss_esp0 );
		PUSHAD	; 32 bitレジスタを全て保存しておく
		MOV		EAX,[ESP+36]	; アプリ用EIP
		MOV		ECX,[ESP+40]	; アプリ用CS
		MOV		EDX,[ESP+44]	; アプリ用ESP
		MOV		EBX,[ESP+48]	; アプリ用DS/SS
		MOV		EBP,[ESP+52]	; tss.esp0の番地
		MOV		[EBP],ESP		; OS用のESPを保存
		MOV		[EBP+4],SS		; OS用のSSを保存
		MOV		ES,BX
		MOV		DS,BX
		MOV		FS,BX
		MOV		GS,BX
	; 以下はRETFでアプリに行かせるためのスタック調整
		OR		ECX,3	; アプリ用のセグメント番号に3をORする
		OR		EBX,3	; アプリ用のセグメント番号に3をORする
		PUSH	EBX		; アプリ用のSS
		PUSH	EDX		; アプリ用のESP
		PUSH	ECX		; アプリ用のCS
		PUSH	EAX		; アプリ用のEIP
		RETF

_asm_hrb_api:
		STI
		PUSH	DS
		PUSH	ES
		PUSHAD	; 保存のためのpush
		PUSHAD	; hrb_apiに戻すためのPUSH
		MOV		AX,SS
		MOV		DS,AX	; OSのセグメントをDSとESにも入れる
		MOV		ES,AX
		CALL	_hrb_api
		CMP		EAX,0	; EAXが0でなければアプリ強制終了
		JNE		_asm_end_app
		ADD		ESP,32
		POPAD
		POP		ES
		POP		DS
		IRETD
_asm_end_app:
	; EAXはtss.esp0の番地
		MOV		ESP,[EAX]
		MOV		DWORD [EAX+4],0
		POPAD
		RET		; cmd_appに戻る

_asm_inthandler0d:
		STI		
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX, ESP
		PUSH	EAX		; 割り込まれたときのEAXを保持
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler0d
		CMP		EAX,0
		JNE		_asm_end_app
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		ADD		ESP,4		; INT 0x0dではこれが必要
		IRETD

_asm_inthandler0c:
		STI		
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX, ESP
		PUSH	EAX		; 割り込まれたときのEAXを保持
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler0c
		CMP		EAX,0
		JNE		_asm_end_app
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		ADD		ESP,4		; INT 0x0dではこれが必要
		IRETD
