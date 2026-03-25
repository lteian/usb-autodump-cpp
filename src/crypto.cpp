#include "crypto.h"
#include <QCryptographicHash>
#include <QByteArray>
#include <QDebug>

const char* Crypto::SALT = "usb_autodump_salt_v1_2024";

QString Crypto::encrypt(const QString& plaintext, const QString& password) {
    if (plaintext.isEmpty() || password.isEmpty()) return QString();

    try {
        // Derive key: SHA256(password + salt), take first 16 bytes
        QByteArray saltBa = QByteArray(SALT);
        QByteArray pwdBa = password.toUtf8() + saltBa;
        QByteArray keyFull = QCryptographicHash::hash(pwdBa, QCryptographicHash::Sha256);
        QByteArray key = keyFull.left(16);

        // XOR encrypt plaintext bytes
        QByteArray plainBa = plaintext.toUtf8();
        QByteArray cipherBa;
        cipherBa.reserve(plainBa.size());
        for (int i = 0; i < plainBa.size(); ++i) {
            cipherBa.append(plainBa[i] ^ key[i % key.size()]);
        }

        return cipherBa.toBase64();
    } catch (...) {
        return QString();
    }
}

QString Crypto::decrypt(const QString& ciphertext, const QString& password) {
    if (ciphertext.isEmpty() || password.isEmpty()) return QString();

    try {
        // Derive same key
        QByteArray saltBa = QByteArray(SALT);
        QByteArray pwdBa = password.toUtf8() + saltBa;
        QByteArray keyFull = QCryptographicHash::hash(pwdBa, QCryptographicHash::Sha256);
        QByteArray key = keyFull.left(16);

        // Decode Base64
        QByteArray cipherBa = QByteArray::fromBase64(ciphertext.toUtf8());

        // XOR decrypt
        QByteArray plainBa;
        plainBa.reserve(cipherBa.size());
        for (int i = 0; i < cipherBa.size(); ++i) {
            plainBa.append(cipherBa[i] ^ key[i % key.size()]);
        }

        return QString::fromUtf8(plainBa);
    } catch (...) {
        return QString();
    }
}
