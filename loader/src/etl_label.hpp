#pragma once
#include "etl_interface.hpp"
#include "params.hpp"
#include "util.hpp"

namespace nervana {

    namespace label {
        class config;
        class decoded;

        class extractor;
        class transformer;
        class loader;
    }

    class label::config : public interface::config {
    public:
        bool binary = true;
        std::string type_string{"uint32_t"};

        config(nlohmann::json js)
        {
            parse_value(binary,      "binary",      js, mode::OPTIONAL);
            parse_value(type_string, "type_string", js, mode::OPTIONAL);

            otype = nervana::output_type(type_string);
            shape = std::vector<uint32_t> {1};

            base_validate();
        }
    };

    class label::decoded : public decoded_media {
    public:
        decoded(int index) :
            _index{index} {}
        virtual ~decoded() override {}

        int get_index() { return _index; }

    private:
        decoded() = delete;
        int _index;
    };


    class label::extractor : public interface::extractor<label::decoded> {
    public:
        extractor(const label::config& cfg)
        {
            _binary = cfg.binary;
        }

        ~extractor() {}

        std::shared_ptr<label::decoded> extract(const char* buf, int bufSize) override
        {
            int lbl;
            if (_binary) {
                if (bufSize != 4) {
                    throw std::runtime_error("Only 4 byte buffers can be loaded as int32");
                }
                lbl = unpack_le<int>(buf);
            } else {
                lbl = std::stoi(std::string(buf, (size_t) bufSize));
            }
            return std::make_shared<label::decoded>(lbl);
        }

    private:
        bool _binary = true;
    };


    class label::transformer : public interface::transformer<label::decoded, nervana::params> {
    public:
        transformer(const label::config&) {}

        ~transformer() {}

        std::shared_ptr<label::decoded> transform(
                            std::shared_ptr<nervana::params> txs,
                            std::shared_ptr<label::decoded> mp) override { return mp; }
    };

    class label::loader : public interface::loader<label::decoded> {
    public:
        loader(const label::config& cfg) : _cfg{cfg} {}
        ~loader() {}

        void load(char* buf, std::shared_ptr<label::decoded> mp) override
        {
            int index = mp->get_index();
            memcpy(buf, &index, _cfg.get_type().size);
        }
    private:
        const label::config& _cfg;
    };
}
