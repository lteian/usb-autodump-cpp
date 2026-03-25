#ifndef CRYPTO_H
#define CRYPTO_H

#include <QString>

class Crypto {
public:
    // XOR-based encryption with PBKDF2-SHA256 key derivation
    // plaintext: original string
    // password: user-set encryption password
    // returns: Base64-encoded ciphertext (or empty on error)
    static QString encrypt(const QString& plaintext, const QString& password);

    // decrypt ciphertext encrypted with encrypt()
    // returns: original plaintext (or empty on error)
    static QString decrypt(const QString& ciphertext, const QString& password);

private:
    static const char* SALT;
};

#endif // CRYPTO_H
