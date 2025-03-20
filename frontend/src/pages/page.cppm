export module frontend.pages:page;

import webxx;

namespace frontend::pages {
    export class page {
        public:
            virtual ~page() = default;
            virtual Webxx::fragment render() = 0;

            virtual void open() {
                is_open = true;
            }
            virtual void close() {
                is_open = false;
            }
        protected:
            bool is_open{false};
    };
}
