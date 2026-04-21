#pragma once

#include "picaresque/article/article_repository.hpp"

namespace picaresque::article {

ArticleRepository& GetMySqlArticleRepository();

}  // namespace picaresque::article
