#pragma once

#include <vector>
#include <memory>
#include <variant>
#include <string>
#include "shapes.h"

Mesh brep_union(const std::vector<std::unique_ptr<IShape>>& shapes);