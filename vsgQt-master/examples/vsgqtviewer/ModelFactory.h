#pragma once

#include <vsg/all.h>
#include <vsg/io/read.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/state/material.h>
#include <vsg/text/Text.h>
#include <vsg/text/Font.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/utils/Builder.h>
#include <vsgXchange/all.h>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <unordered_map>
#include <string>

class FixMaterialVisitor : public vsg::Visitor {
public:
    void apply(vsg::Object& object) override;
    void apply(vsg::StateGroup& sg) override;
};

struct ForceBrightMaterialVisitor : public vsg::Visitor
{
    void apply(vsg::Object& object) override
    {
        object.traverse(*this);
    }

    void apply(vsg::Data& data) override
    {
        if (auto* phongValue = data.cast<vsg::PhongMaterialValue>())
        {
            vsg::PhongMaterial material = phongValue->value();
            material.diffuse   = vsg::vec4(1.0f, 0.95f, 0.8f, 1.0f);
            material.emissive  = vsg::vec4(0.15f, 0.15f, 0.15f, 1.0f);
            material.specular  = vsg::vec4(0.8f, 0.8f, 0.8f, 1.0f);
            material.shininess = 32.0f;
            phongValue->set(material);
        }
        else if (auto* pbrValue = data.cast<vsg::PbrMaterialValue>())
        {
            vsg::PbrMaterial material = pbrValue->value();
            material.baseColorFactor = vsg::vec4(1.0f, 0.95f, 0.8f, 1.0f);
            material.emissiveFactor  = vsg::vec4(0.15f, 0.15f, 0.15f, 1.0f);
            material.metallicFactor  = 0.1f;
            material.roughnessFactor = 0.5f;
            pbrValue->set(material);
        }
        data.traverse(*this);
    }
};

inline void forceBrightMaterial(vsg::Node* node)
{
    ForceBrightMaterialVisitor visitor;
    node->accept(visitor);
}

struct SetTeamColorVisitor : public vsg::Visitor
{
    vsg::vec4 color;
    SetTeamColorVisitor(const vsg::vec4& c) : color(c) {}

    void apply(vsg::Object& object) override { object.traverse(*this); }

    void apply(vsg::DescriptorBuffer& db) override
    {
        for (auto& bufferInfo : db.bufferInfoList)
        {
            if (!bufferInfo || !bufferInfo->data) continue;
            
            if (auto* phong = bufferInfo->data->cast<vsg::PhongMaterialValue>())
            {
                qDebug() << "[Color] Found Phong via DescriptorBuffer, replacing";
                auto newMat = vsg::PhongMaterialValue::create();
                newMat->value() = phong->value();
                newMat->value().diffuse = color;
                newMat->value().emissive = vsg::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                bufferInfo->data = newMat;
            }
            else if (auto* pbr = bufferInfo->data->cast<vsg::PbrMaterialValue>())
            {
                qDebug() << "[Color] Found PBR via DescriptorBuffer, replacing";
                auto newMat = vsg::PbrMaterialValue::create();
                newMat->value() = pbr->value();
                newMat->value().baseColorFactor = color;
                newMat->value().emissiveFactor  = vsg::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                newMat->value().metallicFactor  = 0.0f;
                newMat->value().roughnessFactor = 0.8f;
                bufferInfo->data = newMat;
            }
            else
            {
                qDebug() << "[Color] DescriptorBuffer data type:" 
                         << (bufferInfo->data ? bufferInfo->data->className() : "null");
            }
        }
        db.traverse(*this);
    }
};

inline void applyTeamColor(vsg::Node* node, const std::string& colorName)
{
    vsg::vec4 color;
    if      (colorName == "RedTeam")  color = vsg::vec4(0.8f, 0.05f, 0.05f, 1.0f);  // 深红
    else if (colorName == "BlueTeam") color = vsg::vec4(0.05f, 0.15f, 0.8f, 1.0f);  // 深蓝
    else return; // 未知阵营不染色

    SetTeamColorVisitor visitor(color);
    node->accept(visitor);
}

class ModelFactory {
public:
    static ModelFactory* instance();
    
    void init(vsg::ref_ptr<vsg::Options> options);
    
    vsg::ref_ptr<vsg::Node> getRawModel(const std::string& filename);
    
    vsg::ref_ptr<vsg::Node> createLabel(const QString& text, double height);
    
    vsg::ref_ptr<vsg::Node> createVisualEntity(
        const std::string& filename, 
        const QString& label, 
        double targetSize = 100.0,
        const std::string& teamColor = ""
    );

    vsg::ref_ptr<vsg::Node> getColoredModel(const std::string& filename, const std::string& teamColor);

    
    bool isInitialized() const { return _initialized; }
    bool hasFont() const { return _font.valid(); }
    
private:
    ModelFactory() = default;
    ~ModelFactory() = default;
    ModelFactory(const ModelFactory&) = delete;
    ModelFactory& operator=(const ModelFactory&) = delete;
    
    vsg::ref_ptr<vsg::Font> _font;
    vsg::ref_ptr<vsg::Options> _options;
    std::map<std::string, vsg::ref_ptr<vsg::Node>> _modelCache;
    std::unordered_map<std::string, vsg::ref_ptr<vsg::Node>> _coloredModelCache;
    bool _initialized = false;
    bool _vsgXchangeLoaded = false;
    
    vsg::dbox computeBoundingBox(vsg::ref_ptr<vsg::Node> node);
    double computeScaleFactor(const vsg::dbox& box, double targetSize);
};
