/*
 * This file is part of the Skycoin project, https://skycoin.net/
 *
 * Copyright (C) 2018-2019 Skycoin Project
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "fsm_impl.h"

#include <stdio.h>

#include <libopencm3/stm32/flash.h>


#include "trezor.h"
#include "fsm.h"
#include "messages.h"
#include "bip32.h"
#include "storage.h"
#include "rng.h"
#include "storage.h"
#include "oled.h"
#include "protect.h"
#include "pinmatrix.h"
#include "layout2.h"
#include "base58.h"
#include "reset.h"
#include "recovery.h"
#include "bip39.h"
#include "memory.h"
#include "usb.h"
#include "util.h"
#include "gettext.h"
#include "skycoin_crypto.h"
#include "skycoin_check_signature.h"
#include "check_digest.h"

#define MNEMONIC_STRENGTH_12 128
#define MNEMONIC_STRENGTH_24 256

ErrCode_t msgGenerateMnemonicImpl(GenerateMnemonic* msg) {
 	CHECK_NOT_INITIALIZED_RET_ERR_CODE	    
	int strength = (msg->word_count == 24)? MNEMONIC_STRENGTH_24 : MNEMONIC_STRENGTH_12 ;
	const char* mnemonic = mnemonic_generate(strength);
	if (mnemonic == 0) {
		fsm_sendFailure(FailureType_Failure_ProcessError, _("Device could not generate a Mnemonic"));
		return ErrFailed;
	}
	if (!mnemonic_check(mnemonic)) {
		fsm_sendFailure(FailureType_Failure_DataError, _("Mnemonic with wrong checksum provided"));
		return ErrFailed;
	}
	storage_setMnemonic(mnemonic);
	storage_setNeedsBackup(true);
	storage_setPassphraseProtection(msg->has_passphrase_protection && msg->passphrase_protection);
	storage_update();
	return ErrOk;
}


void msgSkycoinSignMessageImpl(SkycoinSignMessage* msg,
								   ResponseSkycoinSignMessage *resp)
{
	if (storage_hasMnemonic() == false) {
		fsm_sendFailure(FailureType_Failure_AddressGeneration, "Mnemonic not set");
		return;
	}
	CHECK_PIN_UNCACHED
	uint8_t pubkey[33] = {0};
	uint8_t seckey[32] = {0};
	fsm_getKeyPairAtIndex(1, pubkey, seckey, NULL, msg->address_n);
	uint8_t digest[32] = {0};
	if (is_digest(msg->message) == false) {
		compute_sha256sum((const uint8_t *)msg->message, digest, strlen(msg->message));
	} else {
		writebuf_fromhexstr(msg->message, digest);
	}
	uint8_t signature[65];
	int res = ecdsa_skycoin_sign(rand(), seckey, digest, signature);
	if (res == 0) {
		layoutRawMessage("Signature success");
	} else {
		layoutRawMessage("Signature failed");
	}
	const size_t hex_len = 2 * sizeof(signature);
	char signature_in_hex[hex_len];
	tohex(signature_in_hex, signature, sizeof(signature));
	memcpy(resp->signed_message, signature_in_hex, hex_len);
	msg_write(MessageType_MessageType_ResponseSkycoinSignMessage, resp);
	layoutHome();
}

ErrCode_t msgSignTransactionMessageImpl(uint8_t* message_digest, uint32_t index, char* signed_message) {
	uint8_t pubkey[33] = {0};
	uint8_t seckey[32] = {0};
	uint8_t signature[65];
	int res = ErrOk;
	fsm_getKeyPairAtIndex(1, pubkey, seckey, NULL, index);
	if (ecdsa_skycoin_sign(rand(), seckey, message_digest, signature)) {
		res = ErrFailed;
	}
	tohex(signed_message, signature, sizeof(signature));
#if EMULATOR
	printf("Size_sign: %ld, sign58: %s\n", sizeof(signature) * 2, signed_message);
#endif
	return res;
}

ErrCode_t msgSkycoinAddress(SkycoinAddress* msg, ResponseSkycoinAddress *resp)
{
	uint8_t seckey[32] = {0};
	uint8_t pubkey[33] = {0};
	uint32_t start_index = !msg->has_start_index ? 0 : msg->start_index;
	CHECK_PIN_RET_ERR_CODE
	if (msg->address_n > 99) {
		fsm_sendFailure(FailureType_Failure_AddressGeneration, "Asking for too much addresses");
		return ErrFailed;
	}

	if (storage_hasMnemonic() == false) {
		fsm_sendFailure(FailureType_Failure_AddressGeneration, "Mnemonic not set");
		return ErrFailed;
	}

	if (fsm_getKeyPairAtIndex(msg->address_n, pubkey, seckey, resp, start_index) != 0)
	{
		fsm_sendFailure(FailureType_Failure_AddressGeneration, "Key pair generation failed");
		return ErrFailed;
	}
	if (msg->address_n == 1 && msg->has_confirm_address && msg->confirm_address) {
		char * addr = resp->addresses[0];
		layoutAddress(addr);
		if (!protectButton(ButtonRequestType_ButtonRequest_ProtectCall, false)) {
			fsm_sendFailure(FailureType_Failure_ActionCancelled, NULL);
			layoutHome();
			return ErrFailed;
		}
	}
	return ErrOk;
}

ErrCode_t msgSkycoinCheckMessageSignature(SkycoinCheckMessageSignature* msg, Success *resp)
{
	// NOTE(denisacostaq@gmail.com): -1 because the end of string ('\0')
	// /2 because the hex to buff conversion.
	uint8_t sign[(sizeof(msg->signature) - 1)/2];
	// NOTE(denisacostaq@gmail.com): -1 because the end of string ('\0')
	char pubkeybase58[sizeof(msg->address) - 1];
	uint8_t pubkey[33] = {0};
	// NOTE(denisacostaq@gmail.com): -1 because the end of string ('\0')
	// /2 because the hex to buff conversion.
	uint8_t digest[(sizeof(msg->message) - 1) / 2] = {0};
	//     RESP_INIT(Success);
	if (is_digest(msg->message) == false) {
		compute_sha256sum((const uint8_t *)msg->message, digest, strlen(msg->message));
	} else {
		tobuff(msg->message, digest, MIN(sizeof(digest), sizeof(msg->message)));
	}
	tobuff(msg->signature, sign, sizeof(sign));
	recover_pubkey_from_signed_message((char*)digest, sign, pubkey);
	size_t pubkeybase58_size = sizeof(pubkeybase58);
	generate_base58_address_from_pubkey(pubkey, pubkeybase58, &pubkeybase58_size);
	if (memcmp(pubkeybase58, msg->address, pubkeybase58_size) == 0) {
		layoutRawMessage("Verification success");
	} else {
		layoutRawMessage("Wrong signature");
	}
	memcpy(resp->message, pubkeybase58, pubkeybase58_size);
	resp->has_message = true;
	msg_write(MessageType_MessageType_Success, resp);
	return ErrOk;
}
