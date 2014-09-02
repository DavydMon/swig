#
# Be sure to run `pod lib lint Swig.podspec' to ensure this is a
# valid spec and remove all comments before submitting the spec.
#
# Any lines starting with a # are optional, but encouraged
#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html
#

Pod::Spec.new do |s|
  s.name             = "Swig"
  s.version          = "0.1.0"
  s.summary          = "PJSIP Wrapper for ios"
  s.description      = <<-DESC
                       Simplifing the use of pjsip on ios
                       DESC
  s.homepage         = "https://github.com/petester42/swig"
  s.license          = 'GPLv2'
  s.author           = { "Pierre-Marc Airoldi" => "pierremarcairoldi@gmail.com" }
  s.source           = { :git => "https://github.com/petester42/swig.git", :tag => s.version.to_s }
  s.social_media_url = 'https://twitter.com/petester42'

  s.platform     = :ios, '7.0'
  s.requires_arc = true

  s.source_files = 'Pod/Classes/**/*{h,m}'
  s.preserve_paths = 'Pod/Classes/**/*{h,m}'

  s.dependency 'AFNetworking/Reachability', '~> 2.3'
  s.dependency 'pjsip-ios'

  s.header_mappings_dir = 'Pod/Classes'
  
  s.xcconfig = {
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited)',
    'HEADER_SEARCH_PATHS'  => '$(PODS_ROOT)/pjsip-ios/Pod/pjsip-include',
    'LIBRARY_SEARCH_PATHS' => '$(PODS_ROOT)/pjsip-ios/Pod/pjsip-lib'
  }
end