#include "bt_open_pairing_allowlist.h"

#include <furi.h>
#include <storage/storage.h>

#define TAG "BtAllow"

// Internal flash, not the SD card: the list must survive a missing or swapped card, and it must
// not travel with the card to another device.
#define ALLOWLIST_PATH INT_PATH(".bt_open_allow")
#define ALLOWLIST_MAGIC 0xA1
#define ALLOWLIST_VERSION 1

typedef struct {
    uint8_t addr_type;
    uint8_t addr[BT_OPEN_PAIRING_ADDR_SIZE];
} __attribute__((packed)) AllowEntry;

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint8_t count;
    AllowEntry entries[BT_OPEN_PAIRING_ALLOWLIST_MAX];
} __attribute__((packed)) AllowFile;

static bool allowlist_load(AllowFile* out) {
    memset(out, 0, sizeof(*out));
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, ALLOWLIST_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t read = storage_file_read(file, out, sizeof(*out));
        ok = read >= 3 && out->magic == ALLOWLIST_MAGIC && out->version == ALLOWLIST_VERSION &&
             out->count <= BT_OPEN_PAIRING_ALLOWLIST_MAX;
        if(!ok) {
            FURI_LOG_W(TAG, "Allowlist file invalid, ignoring");
            memset(out, 0, sizeof(*out));
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool allowlist_save(const AllowFile* in) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, ALLOWLIST_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t len = 3 + in->count * sizeof(AllowEntry);
        ok = storage_file_write(file, in, len) == len;
    }
    if(!ok) FURI_LOG_E(TAG, "Failed to save allowlist");
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool bt_open_pairing_allowlist_contains(
    uint8_t addr_type,
    const uint8_t addr[BT_OPEN_PAIRING_ADDR_SIZE]) {
    AllowFile f;
    if(!allowlist_load(&f)) return false;
    for(uint8_t i = 0; i < f.count; i++) {
        if(f.entries[i].addr_type == addr_type &&
           memcmp(f.entries[i].addr, addr, BT_OPEN_PAIRING_ADDR_SIZE) == 0) {
            return true;
        }
    }
    return false;
}

void bt_open_pairing_allowlist_add(
    uint8_t addr_type,
    const uint8_t addr[BT_OPEN_PAIRING_ADDR_SIZE]) {
    AllowFile f;
    allowlist_load(&f);
    f.magic = ALLOWLIST_MAGIC;
    f.version = ALLOWLIST_VERSION;
    for(uint8_t i = 0; i < f.count; i++) {
        if(f.entries[i].addr_type == addr_type &&
           memcmp(f.entries[i].addr, addr, BT_OPEN_PAIRING_ADDR_SIZE) == 0) {
            return; // already there
        }
    }
    if(f.count >= BT_OPEN_PAIRING_ALLOWLIST_MAX) {
        // Full: drop the oldest. Eight approved watches is already far more than one person owns;
        // the alternative — refusing to remember a new one — would prompt on every connection.
        memmove(&f.entries[0], &f.entries[1], (f.count - 1) * sizeof(AllowEntry));
        f.count--;
    }
    f.entries[f.count].addr_type = addr_type;
    memcpy(f.entries[f.count].addr, addr, BT_OPEN_PAIRING_ADDR_SIZE);
    f.count++;
    allowlist_save(&f);
    FURI_LOG_I(TAG, "Approved central stored (%u total)", f.count);
}

void bt_open_pairing_allowlist_clear(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, ALLOWLIST_PATH);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "Allowlist cleared");
}

uint8_t bt_open_pairing_allowlist_count(void) {
    AllowFile f;
    if(!allowlist_load(&f)) return 0;
    return f.count;
}
