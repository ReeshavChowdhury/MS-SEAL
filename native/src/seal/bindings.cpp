#ifdef EMSCRIPTEN

#include <emscripten/bind.h>
#include <cstdint>
#include <stdexcept>
#include <iomanip>
#include <typeinfo>

#include "seal.h"

using namespace std;
using namespace emscripten;
using namespace seal;

template<typename T>
std::vector<T> vecFromArray(const val &v) {
    auto l = v["length"].as<unsigned>();

    std::vector<T> rv;
    rv.reserve(l);

    if (internal::typeSupportsMemoryView<T>()) {
        rv.resize(l);

        const char *arrayType;

//        cout << "type of array: " << typeid(T).name() << endl;
        switch (sizeof(T)) {
            case 1:
                if (std::is_unsigned<T>::value) {
//                    cout << "Creating Uint8Array" << endl;
                    arrayType = "Uint8Array";
                } else {
//                    cout << "Creating Int8Array" << endl;
                    arrayType = "Int8Array";
                }
                break;
            case 2:
                if (std::is_unsigned<T>::value) {
//                    cout << "Creating Uint16Array" << endl;
                    arrayType = "Uint16Array";
                } else {
//                    cout << "Creating Int16Array" << endl;
                    arrayType = "Int16Array";
                }
                break;
            case 4:
                if (std::is_unsigned<T>::value) {
//                    cout << "Creating Uint32Array" << endl;
                    arrayType = "Uint32Array";
                } else if (std::is_integral<T>::value) {
//                    cout << "Creating Int32Array" << endl;
                    arrayType = "Int32Array";
                } else {
//                    cout << "Creating Float32Array" << endl;
                    arrayType = "Float32Array";
                }
                break;
            case 8:
//                cout << "Creating Float64Array" << endl;
                arrayType = "Float64Array";
                break;
            default:
                //This should be never reached if the 'typeSupportsMemoryView' function returns true.
                throw std::logic_error("Unreachable");
        }

        val memory = val::module_property("buffer");
        val memoryView = val::global(arrayType).new_(memory, reinterpret_cast<std::uintptr_t>(rv.data()), l);

        memoryView.call<void>("set", v);
    } else {
        for (unsigned i = 0; i < l; ++i) {
            rv.push_back(v[i].as<T>());
        }
    }

    return rv;
};

/*
Helper function: Prints a vector of floating-point values.
*/
template<typename T>
void printVector(std::vector<T> vec, size_t print_size = 4, int prec = 3)
{
    /*
    Save the formatting information for std::cout.
    */
    ios old_fmt(nullptr);
    old_fmt.copyfmt(cout);

    size_t slot_count = vec.size();

    cout << fixed << setprecision(prec) << endl;
    if(slot_count <= 2 * print_size)
    {
        cout << "    [";
        for (size_t i = 0; i < slot_count; i++)
        {
            cout << " " << vec[i] << ((i != slot_count - 1) ? "," : " ]\n");
        }
    }
    else
    {
        vec.resize(max(vec.size(), 2 * print_size));
        cout << "    [";
        for (size_t i = 0; i < print_size; i++)
        {
            cout << " " << vec[i] << ",";
        }
        if(vec.size() > 2 * print_size)
        {
            cout << " ...,";
        }
        for (size_t i = slot_count - print_size; i < slot_count; i++)
        {
            cout << " " << vec[i] << ((i != slot_count - 1) ? "," : " ]\n");
        }
    }
    cout << endl;

    /*
    Restore the old std::cout formatting.
    */
    cout.copyfmt(old_fmt);
}

/*
Printing the matrix is a bit of a pain.
*/
template<typename T>
void printMatrix(std::vector<T> &matrix, size_t row_size)
{
    /*
    Save the formatting information for std::cout.
    */
    ios old_fmt(nullptr);
    old_fmt.copyfmt(cout);

    cout << endl;

    /*
    We're not going to print every column of the matrix (there are 2048). Instead
    print this many slots from beginning and end of the matrix.
    */
    size_t print_size = 5;

    cout << "    [";
    for (size_t i = 0; i < print_size; i++)
    {
        cout << setw(3) << matrix[i] << ",";
    }
    cout << setw(3) << " ...,";
    for (size_t i = row_size - print_size; i < row_size; i++)
    {
        cout << setw(3) << matrix[i] << ((i != row_size - 1) ? "," : " ]\n");
    }
    cout << "    [";
    for (size_t i = row_size; i < row_size + print_size; i++)
    {
        cout << setw(3) << matrix[i] << ",";
    }
    cout << setw(3) << " ...,";
    for (size_t i = 2 * row_size - print_size; i < 2 * row_size; i++)
    {
        cout << setw(3) << matrix[i] << ((i != 2 * row_size - 1) ? "," : " ]\n");
    }
    cout << endl;

    /*
    Restore the old std::cout formatting.
    */
    cout.copyfmt(old_fmt);
}

/*
Helper function: Prints the parameters in a SEALContext.
*/
void printContext(shared_ptr<SEALContext> context)
{
    // Verify parameters
    if (!context)
    {
        throw invalid_argument("context is not set");
    }
    auto &context_data = *context->context_data();

    /*
    Which scheme are we using?
    */
    string scheme_name;
    switch (context_data.parms().scheme())
    {
    case scheme_type::BFV:
        scheme_name = "BFV";
        break;
    case scheme_type::CKKS:
        scheme_name = "CKKS";
        break;
    default:
        throw invalid_argument("unsupported scheme");
    }

    cout << "/ Encryption parameters:" << endl;
    cout << "| scheme: " << scheme_name << endl;
    cout << "| poly_modulus_degree: " <<
        context_data.parms().poly_modulus_degree() << endl;

    /*
    Print the size of the true (product) coefficient modulus.
    */
    cout << "| coeff_modulus size: " << context_data.
        total_coeff_modulus_bit_count() << " bits" << endl;

    /*
    For the BFV scheme print the plain_modulus parameter.
    */
    if (context_data.parms().scheme() == scheme_type::BFV)
    {
        cout << "| plain_modulus: " << context_data.
            parms().plain_modulus().value() << endl;
    }

    cout << "\\ noise_standard_deviation: " << context_data.
        parms().noise_standard_deviation() << endl;
    cout << endl;
}

EMSCRIPTEN_BINDINGS(bindings)
{

    emscripten::function("printContext", &printContext);
    emscripten::function("vecFromArrayInt32", select_overload<std::vector<int32_t>(const val &)>(&vecFromArray));
    emscripten::function("vecFromArrayUInt32", select_overload<std::vector<uint32_t>(const val &)>(&vecFromArray));
    emscripten::function("vecFromArrayInt64", select_overload<std::vector<int64_t>(const val &)>(&vecFromArray));
    emscripten::function("vecFromArrayUInt64", select_overload<std::vector<uint64_t>(const val &)>(&vecFromArray));
    emscripten::function("vecFromArrayDouble", select_overload<std::vector<double>(const val &)>(&vecFromArray));
    emscripten::function("printVectorInt32", select_overload<void(std::vector<int32_t>, size_t, int)>(&printVector));
    emscripten::function("printVectorUInt32", select_overload<void(std::vector<uint32_t>, size_t, int)>(&printVector));
    emscripten::function("printVectorInt64", select_overload<void(std::vector<int64_t>, size_t, int)>(&printVector));
    emscripten::function("printVectorUInt64", select_overload<void(std::vector<uint64_t>, size_t, int)>(&printVector));
    emscripten::function("printVectorDouble", select_overload<void(std::vector<double>, size_t, int)>(&printVector));
    emscripten::function("printVectorComplexDouble", select_overload<void(std::vector<std::complex<double>>, size_t, int)>(&printVector));
    emscripten::function("printMatrixInt32", select_overload<void(std::vector<int32_t> &, size_t)>(&printMatrix));
    emscripten::function("printMatrixUInt32", select_overload<void(std::vector<uint32_t> &, size_t)>(&printMatrix));

    register_vector<SmallModulus>("std::vector<SmallModulus>");
    register_vector<int32_t>("std::vector<int32_t>");
    register_vector<uint32_t>("std::vector<uint32_t>");
    register_vector<int64_t>("std::vector<int64_t>");
    register_vector<uint64_t>("std::vector<uint64_t>");
    register_vector<double>("std::vector<double>");
    register_vector<std::complex<double>>("std::vector<std::complex<double>>");

    // TODO: Finish defining this item as it just returns an empty array in the JS side
    // using sha3_block_type = std::array<std::uint64_t, sha3_block_uint64_count>;
    value_array<parms_id_type>("parms_id_type")
        // .element(&Point2f::y)
        ;


    class_<DefaultParams>("DefaultParams")
        .class_function("coeffModulus128", &DefaultParams::coeff_modulus_128)
        .class_function("coeffModulus192", &DefaultParams::coeff_modulus_192)
        .class_function("coeffModulus256", &DefaultParams::coeff_modulus_256)
        .class_function("smallMods60bit", &DefaultParams::small_mods_60bit)
        .class_function("smallMods50bit", &DefaultParams::small_mods_50bit)
        .class_function("smallMods40bit", &DefaultParams::small_mods_40bit)
        .class_function("smallMods30bit", &DefaultParams::small_mods_30bit)
        .class_function("dbcMax", &DefaultParams::dbc_max)
        .class_function("dbcMin", &DefaultParams::dbc_min)
        ;

    class_<SmallModulus>("SmallModulus")
        .constructor<>()
        .function("bitCount", &SmallModulus::bit_count)
        .function("value", &SmallModulus::Value)
        .function("loadFromString", &SmallModulus::LoadFromString)
        .class_function("saveToString", &SmallModulus::SaveToString)
        .class_function("createFromString", &SmallModulus::CreateFromString)
        ;


    class_<EncryptionParameters>("EncryptionParameters")
        .constructor<scheme_type>()
        .function("setPolyModulusDegree", &EncryptionParameters::set_poly_modulus_degree)
        .function("setCoeffModulus", &EncryptionParameters::set_coeff_modulus)
        .function("setPlainModulus", select_overload<void(const SmallModulus &)>(&EncryptionParameters::set_plain_modulus))
        .class_function("saveToString", &EncryptionParameters::SaveToString)
        .class_function("createFromString", &EncryptionParameters::CreateFromString)
        ;

    class_<SEALContext::ContextData>("SEALContext::ContextData")
        .smart_ptr<std::shared_ptr<SEALContext::ContextData>>("std::shared_ptr<SEALContext::ContextData>")
        .function("parms", &SEALContext::ContextData::parms)
        ;

    class_<SEALContext>("SEALContext")
        .smart_ptr_constructor("std::shared_ptr<SEALContext>", &SEALContext::Create)
        // TODO: Get this helper to work so we can validate our context more easily
//        .function("contextData", select_overload<const std::shared_ptr<const SEALContext::ContextData>(parms_id_type)>
//        (&SEALContext::context_data))

        .function("firstParmsId", &SEALContext::first_parms_id)
        .function("lastParmsId", &SEALContext::last_parms_id)
        .function("parametersSet", &SEALContext::parameters_set)
        ;

    class_<Evaluator>("Evaluator")
        .constructor<std::shared_ptr<SEALContext>>()
        .function("negate", &Evaluator::negate)
        .function("add", &Evaluator::add)
        .function("sub", &Evaluator::sub)
        .function("multiply", &Evaluator::multiply)
        .function("square", &Evaluator::square)
        .function("relinearize", &Evaluator::relinearize)
        .function("exponentiate", &Evaluator::exponentiate)
        .function("addPlain", &Evaluator::add_plain)
        .function("subPlain", &Evaluator::sub_plain)
        .function("multiplyPlain", &Evaluator::multiply_plain)
        .function("applyGalois", &Evaluator::apply_galois)
        .function("rotateRows", &Evaluator::rotate_rows)
        .function("rotateColumns", &Evaluator::rotate_columns)
        .function("rotateVector", &Evaluator::rotate_vector)
        .function("complexConjugate", &Evaluator::complex_conjugate)
        ;

    class_<KeyGenerator>("KeyGenerator")
        .constructor<std::shared_ptr<SEALContext>>()
        .constructor<std::shared_ptr<SEALContext>, const SecretKey &>()
        .constructor<std::shared_ptr<SEALContext>, const SecretKey &, const PublicKey &>()
        .function("getPublicKey", &KeyGenerator::public_key)
        .function("getSecretKey", &KeyGenerator::secret_key)
        .function("createRelinKeys", select_overload<RelinKeys(int, std::size_t)>(&KeyGenerator::relin_keys))
        .function("createGaloisKeys", select_overload<GaloisKeys(int)>(&KeyGenerator::galois_keys))
        ;

    class_<PublicKey>("PublicKey")
        .constructor<>()
        .function("saveToString", &PublicKey::SaveToString)
        .function("loadFromString", &PublicKey::LoadFromString)
        ;

    class_<SecretKey>("SecretKey")
        .constructor<>()
        .function("saveToString", &SecretKey::SaveToString)
        .function("loadFromString", &SecretKey::LoadFromString)
        ;

    class_<RelinKeys>("RelinKeys")
        .constructor<>()
        .function("saveToString", &RelinKeys::SaveToString)
        .function("loadFromString", &RelinKeys::LoadFromString)
        ;

    class_<GaloisKeys>("GaloisKeys")
        .constructor<>()
        .function("saveToString", &GaloisKeys::SaveToString)
        .function("loadFromString", &GaloisKeys::LoadFromString)
        ;

    class_<Plaintext>("Plaintext")
        .constructor<>()
        .function("toString", &Plaintext::to_string)
        ;
    class_<Ciphertext>("Ciphertext")
        .constructor<>()
        .function("saveToString", &Ciphertext::SaveToString)
        .function("loadFromString", &Ciphertext::LoadFromString)
        .function("pool", &Ciphertext::pool)
        .function("scale", select_overload< double & ()>(&Ciphertext::scale))
        .function("parmsId", select_overload< parms_id_type & ()>(&Ciphertext::parms_id))
        ;

    class_<CKKSEncoder>("CKKSEncoder")
        .constructor<std::shared_ptr<SEALContext>>()
        .function("encodeVectorDouble", select_overload<void(const std::vector<double> &, double, Plaintext &, MemoryPoolHandle)>(&CKKSEncoder::encodeVector))
        .function("encodeVectorComplexDouble", select_overload<void(const std::vector<std::complex<double>> &, double, Plaintext &, MemoryPoolHandle)>(&CKKSEncoder::encodeVector))
        .function("decodeVectorDouble", select_overload<void(const Plaintext &, std::vector<double> &, MemoryPoolHandle)>(&CKKSEncoder::decodeVector))
        .function("decodeVectorComplexDouble", select_overload<void(const Plaintext &, std::vector<std::complex<double>> &, MemoryPoolHandle)>(&CKKSEncoder::decodeVector))
        ;

    class_<IntegerEncoder>("IntegerEncoder")
        .constructor<std::shared_ptr<SEALContext>>()
        .function("encodeInt32", select_overload<Plaintext(std::int32_t)>(&IntegerEncoder::encode))
        .function("encodeInt64", select_overload<Plaintext(std::int64_t)>(&IntegerEncoder::encode))
        .function("encodeUInt32", select_overload<Plaintext(std::uint32_t)>(&IntegerEncoder::encode))
        .function("encodeUInt64", select_overload<Plaintext(std::uint64_t)>(&IntegerEncoder::encode))
        .function("encodeBigUInt", select_overload<Plaintext(const BigUInt &)>(&IntegerEncoder::encode))

        .function("decodeInt32", select_overload<std::int32_t(const Plaintext &)>(&IntegerEncoder::decode_int32))
        .function("decodeInt64", select_overload<std::int64_t(const Plaintext &)>(&IntegerEncoder::decode_int64))
        .function("decodeUInt32", select_overload<std::uint32_t(const Plaintext &)>(&IntegerEncoder::decode_uint32))
        .function("decodeUInt64", select_overload<std::uint64_t(const Plaintext &)>(&IntegerEncoder::decode_uint64))
        .function("decodeBigUInt", select_overload<BigUInt(const Plaintext &)>(&IntegerEncoder::decode_biguint))
        ;

    class_<BatchEncoder>("BatchEncoder")
        .constructor<std::shared_ptr<SEALContext>>()
        .function("encodeVectorInt32", select_overload<void(const std::vector<std::int32_t> &, Plaintext &)>(&BatchEncoder::encodeVector))
        .function("encodeVectorUInt32", select_overload<void(const std::vector<std::uint32_t> &, Plaintext &)>(&BatchEncoder::encodeVector))
        .function("decodeVectorInt32", select_overload<void(const Plaintext &, std::vector<std::int32_t> &,
        MemoryPoolHandle)>(&BatchEncoder::decodeVector))
        .function("decodeVectorUInt32", select_overload<void(const Plaintext &, std::vector<std::uint32_t> &,
        MemoryPoolHandle)>(&BatchEncoder::decodeVector))
        .function("slotCount", &BatchEncoder::slot_count)
        ;

    class_<MemoryPoolHandle>("MemoryPoolHandle")
        .constructor<>()
        .class_function("MemoryPoolHandleGlobal", &MemoryPoolHandle::Global)
        .class_function("MemoryPoolHandleThreadLocal", &MemoryPoolHandle::ThreadLocal)
        .class_function("MemoryPoolHandleNew", &MemoryPoolHandle::New)
        ;

    class_<MemoryManager>("MemoryManager")
        ;

    class_<MMProf>("MMProf")
        ;

    class_<MMProfGlobal, base<MMProf>>("MMProfGlobal")
        ;

    class_<MMProfNew, base<MMProf>>("MMProfNew")
        ;

    class_<MMProfFixed, base<MMProf>>("MMProfFixed")
        ;

    class_<MMProfThreadLocal, base<MMProf>>("MMProfThreadLocal")
        ;

    class_<Encryptor>("Encryptor")
        .constructor<std::shared_ptr<SEALContext>, const PublicKey &>()
        .function("encrypt", &Encryptor::encrypt)
        ;
    class_<Decryptor>("Decryptor")
        .constructor<std::shared_ptr<SEALContext>, const SecretKey &>()
        .function("decrypt", &Decryptor::decrypt)
        .function("invariantNoiseBudget", &Decryptor::invariant_noise_budget)
        ;

    enum_<scheme_type>("SchemeType")
        .value("BFV", scheme_type::BFV)
        .value("CKKS", scheme_type::CKKS)
        ;
}

#endif
